# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

"""Dynamic, safe queries over rasdaemon event tables."""

from __future__ import annotations

import collections
import datetime
import logging
import re
from dataclasses import dataclass
from typing import Any, Iterable, Mapping

from sqlalchemy import Table, func, or_, select
from sqlalchemy.engine import Connection


@dataclass(frozen=True)
class DatabaseEvent:
    """One record read from a reflected rasdaemon event table."""

    table: str
    hostname: str
    timestamp: Any
    values: Mapping[str, Any]


@dataclass(frozen=True)
class DatabaseFilter:
    """One validated database field comparison."""

    field: str
    operator: str
    value: str


@dataclass(frozen=True)
class DatabaseOrder:
    """One validated result ordering term."""

    field: str
    descending: bool = False


@dataclass(frozen=True)
class DatabaseCount:
    """An aggregate count and the values that identify its group."""

    values: Mapping[str, Any]
    count: int


FIELD_NAME = re.compile(r"[A-Za-z_][A-Za-z0-9_]*\Z")
FILTER_SPEC = re.compile(
    r"\s*([A-Za-z_][A-Za-z0-9_]*)\s*(<=|>=|!=|=|<|>)\s*(\S(?:.*\S)?)\s*\Z"
)
ORDER_SPEC = re.compile(
    r"\s*([A-Za-z_][A-Za-z0-9_]*)(?::(asc|desc))?\s*\Z", re.IGNORECASE
)
VIRTUAL_FIELDS = {"table", "hostname", "timestamp"}

# Severity names recorded by the EDAC and GHES handlers use more than one
# column name. Numeric encodings are table-specific, so list those explicitly
# instead of guessing based only on a column called "severity".
TEXT_SEVERITIES = {
    "corrected": ("corrected",),
    "uncorrected": ("uncorrected",),
    "deferred": ("deferred",),
    "fatal": ("fatal",),
    "info": ("info", "informational"),
    "recoverable": ("recoverable",),
}
NUMERIC_SEVERITIES = {
    "extlog_event": {
        "recoverable": 0,
        "fatal": 1,
        "corrected": 2,
        "info": 3,
    },
    "reri_event": {
        "info": 0,
        "corrected": 1,
        "recoverable": 2,
        "fatal": 3,
    },
}


class RasDatabaseQuery:
    """Build and execute one dynamic query across reflected event tables."""

    def __init__(self, backend: str, hostname: str,
                 tables: Mapping[str, Table], query_logger: logging.Logger) -> None:
        self.backend = backend
        self.hostname = hostname
        self.tables = tables
        self.logger = query_logger

    @staticmethod
    def validate_field(field: str) -> str:
        """Validate a database or virtual field name."""

        if not FIELD_NAME.fullmatch(field):
            raise ValueError(f"invalid database field: {field}")
        return field

    @classmethod
    def parse_filters(cls, specifications: Iterable[str]
                      ) -> tuple[DatabaseFilter, ...]:
        """Parse ``FIELD OP VALUE`` filters without accepting SQL syntax."""

        filters = []
        for specification in specifications:
            match = FILTER_SPEC.fullmatch(specification)
            if not match:
                raise ValueError(
                    "invalid filter {!r}; use FIELD=VALUE or FIELD>=VALUE".format(
                        specification
                    )
                )
            field, operator, value = match.groups()
            filters.append(DatabaseFilter(field, operator, value))
        return tuple(filters)

    @classmethod
    def parse_ordering(cls, specifications: Iterable[str]
                       ) -> tuple[DatabaseOrder, ...]:
        """Parse ``FIELD[:asc|desc]`` ordering terms."""

        ordering = []
        for specification in specifications:
            match = ORDER_SPEC.fullmatch(specification)
            if not match:
                raise ValueError(
                    "invalid ordering {!r}; use FIELD, FIELD:asc, or FIELD:desc".format(
                        specification
                    )
                )
            field, direction = match.groups()
            ordering.append(DatabaseOrder(
                field, direction and direction.lower() == "desc"
            ))
        return tuple(ordering)

    @staticmethod
    def _column_is_text(column: Any) -> bool:
        try:
            return column.type.python_type is str
        except (AttributeError, NotImplementedError):
            return False

    @staticmethod
    def _coerce_filter_value(column: Any, value: str) -> Any:
        """Convert scalar values only when a database type requires it."""

        try:
            python_type = column.type.python_type
        except (AttributeError, NotImplementedError):
            return value
        if python_type is int:
            return int(value, 0)
        if python_type is float:
            return float(value)
        if python_type is bool:
            values = {"true": True, "yes": True, "1": True,
                      "false": False, "no": False, "0": False}
            try:
                return values[value.lower()]
            except KeyError as error:
                raise ValueError(f"invalid boolean value: {value}") from error
        return value

    @classmethod
    def _comparison(cls, column: Any, operator: str, value: str) -> Any:
        value = cls._coerce_filter_value(column, value)
        comparisons = {
            "=": column == value,
            "!=": column != value,
            "<": column < value,
            "<=": column <= value,
            ">": column > value,
            ">=": column >= value,
        }
        return comparisons[operator]

    @staticmethod
    def _literal_matches(value: str, operator: str, expected: str) -> bool:
        comparisons = {
            "=": value == expected,
            "!=": value != expected,
            "<": value < expected,
            "<=": value <= expected,
            ">": value > expected,
            ">=": value >= expected,
        }
        return comparisons[operator]

    def _severity_condition(self, table_name: str, table: Table,
                            severity: str | None) -> Any | None:
        """Return the table-specific predicate for one semantic severity."""

        if not severity:
            return None

        conditions = []
        text_values = TEXT_SEVERITIES[severity]
        for field in ("err_type", "severity", "err_severity"):
            if field not in table.c or not self._column_is_text(table.c[field]):
                continue
            values = list(text_values)
            if table_name == "aer_event":
                if severity == "uncorrected":
                    values.extend(("uncorrected (non-fatal)",
                                   "uncorrected (fatal)"))
                elif severity == "fatal":
                    values.append("uncorrected (fatal)")
            conditions.append(func.lower(table.c[field]).in_(values))

        numeric = NUMERIC_SEVERITIES.get(table_name, {}).get(severity)
        if numeric is not None and "severity" in table.c:
            conditions.append(table.c.severity == numeric)

        if not conditions:
            return None
        return conditions[0] if len(conditions) == 1 else or_(*conditions)

    def _query_conditions(
            self, table_name: str, table: Table, *, since: str | None,
            until: str | None, hostname: str | None,
            filters: Iterable[DatabaseFilter], severity: str | None,
            required_fields: Iterable[str]) -> tuple[list[Any] | None, str | None]:
        """Build safe predicates or explain why a table is incompatible."""

        for field in required_fields:
            if field not in VIRTUAL_FIELDS and field not in table.c:
                return None, f"field '{field}' is absent"

        conditions = []
        for query_filter in filters:
            field = query_filter.field
            if field == "table":
                if not self._literal_matches(
                        table_name, query_filter.operator, query_filter.value):
                    return None, "table filter does not match"
                continue
            if field == "hostname":
                if self.backend != "sqlite3" and "hostname" in table.c:
                    conditions.append(self._comparison(
                        table.c.hostname, query_filter.operator, query_filter.value
                    ))
                elif not self._literal_matches(
                        self.hostname, query_filter.operator, query_filter.value):
                    return None, "hostname filter does not match"
                continue
            if field not in table.c:
                return None, f"field '{field}' is absent"
            conditions.append(self._comparison(
                table.c[field], query_filter.operator, query_filter.value
            ))

        if since:
            conditions.append(table.c.timestamp >= since)
        if until:
            conditions.append(table.c.timestamp <= until)
        if hostname and self.backend != "sqlite3":
            if "hostname" not in table.c:
                return None, "hostname is absent"
            conditions.append(table.c.hostname == hostname)

        severity_condition = self._severity_condition(table_name, table, severity)
        if severity and severity_condition is None:
            return None, f"does not record {severity} severity"
        if severity_condition is not None:
            conditions.append(severity_condition)
        return conditions, None

    def _query_tables(
            self, *, since: str | None, until: str | None,
            hostname: str | None, filters: Iterable[DatabaseFilter],
            severity: str | None, required_fields: Iterable[str]
            ) -> Iterable[tuple[str, Table, list[Any]]]:
        """Yield selected tables that can satisfy this dynamic query."""

        for table_name, table in self.tables.items():
            conditions, reason = self._query_conditions(
                table_name, table, since=since, until=until, hostname=hostname,
                filters=filters, severity=severity, required_fields=required_fields,
            )
            if conditions is None:
                self.logger.debug("Skipping event table %s: %s", table_name, reason)
                continue
            self.logger.debug("Using event table %s", table_name)
            yield table_name, table, conditions

    @staticmethod
    def event_value(event: DatabaseEvent, field: str) -> Any:
        """Return a physical or virtual field from one event."""

        if field == "table":
            return event.table
        if field == "hostname":
            return event.hostname
        if field == "timestamp":
            return event.timestamp
        return event.values.get(field)

    @staticmethod
    def _sort_value(value: Any) -> tuple[int, int, Any]:
        """Return a stable key for values from heterogeneous event tables."""

        if value is None:
            return (1, 0, "")
        if isinstance(value, bool):
            return (0, 0, int(value))
        if isinstance(value, (int, float)):
            return (0, 0, value)
        if isinstance(value, (datetime.date, datetime.datetime)):
            return (0, 1, value.isoformat())
        return (0, 2, str(value))

    def _sort_records(self, events: list[DatabaseEvent],
                      order_by: Iterable[DatabaseOrder]) -> None:
        terms = tuple(order_by) or (
            DatabaseOrder("timestamp"), DatabaseOrder("table"),
            DatabaseOrder("id"),
        )
        if any(term.field == "count" for term in terms):
            raise ValueError("count can only be used to order --count results")
        for term in reversed(terms):
            events.sort(
                key=lambda event, field=term.field: self._sort_value(
                    self.event_value(event, field)
                ),
                reverse=term.descending,
            )

    def records(self, connection: Connection, *, since: str | None = None,
                until: str | None = None, hostname: str | None = None,
                filters: Iterable[DatabaseFilter] = (),
                severity: str | None = None,
                select_fields: Iterable[str] = (),
                order_by: Iterable[DatabaseOrder] = (),
                ) -> dict[str, list[DatabaseEvent]]:
        """Return matching records grouped by hostname."""

        filters = tuple(filters)
        select_fields = tuple(select_fields)
        order_by = tuple(order_by)
        required_fields = list(select_fields)
        required_fields.extend(
            term.field for term in order_by if term.field != "count"
        )
        grouped: dict[str, list[DatabaseEvent]] = collections.defaultdict(list)
        for table_name, table, conditions in self._query_tables(
                since=since, until=until, hostname=hostname, filters=filters,
                severity=severity, required_fields=required_fields):
            statement = select(table).where(*conditions)
            for row in connection.execute(statement).mappings():
                values = dict(row)
                event_hostname = values.get("hostname") or self.hostname
                grouped[str(event_hostname)].append(DatabaseEvent(
                    table_name, str(event_hostname), values.get("timestamp"),
                    values
                ))

        for events in grouped.values():
            self._sort_records(events, order_by)
        return dict(sorted(grouped.items()))

    def counts(self, connection: Connection, *, since: str | None = None,
               until: str | None = None, hostname: str | None = None,
               filters: Iterable[DatabaseFilter] = (),
               severity: str | None = None,
               group_by: Iterable[str] = (),
               order_by: Iterable[DatabaseOrder] = ()
               ) -> list[DatabaseCount]:
        """Count matching events, optionally grouped by runtime fields."""

        filters = tuple(filters)
        group_by = tuple(group_by)
        order_by = tuple(order_by)
        if len(set(group_by)) != len(group_by):
            raise ValueError("each --group-by field may be used only once")
        for field in group_by:
            self.validate_field(field)
        allowed_order = set(group_by) | {"count"}
        for term in order_by:
            if term.field not in allowed_order:
                raise ValueError(
                    "--count can only order by count or a --group-by field"
                )

        grouped: dict[tuple[Any, ...], int] = collections.defaultdict(int)
        for table_name, table, conditions in self._query_tables(
                since=since, until=until, hostname=hostname, filters=filters,
                severity=severity, required_fields=group_by):
            columns = []
            constant_values = {}
            for field in group_by:
                if field == "table":
                    constant_values[field] = table_name
                elif field == "hostname":
                    if self.backend != "sqlite3" and "hostname" in table.c:
                        columns.append((field, table.c.hostname))
                    else:
                        constant_values[field] = self.hostname
                else:
                    columns.append((field, table.c[field]))

            statement = select(func.count()).where(*conditions)
            for _, column in columns:
                statement = statement.add_columns(column).group_by(column)
            for row in connection.execute(statement):
                count = int(row[0])
                if group_by and not count:
                    continue
                values = dict(constant_values)
                for index, (field, _) in enumerate(columns, start=1):
                    values[field] = row[index]
                key = tuple(values[field] for field in group_by)
                grouped[key] += count

        results = [
            DatabaseCount(dict(zip(group_by, key)), count)
            for key, count in grouped.items()
        ]
        if not group_by:
            results = [DatabaseCount({}, sum(item.count for item in results))]

        terms = order_by or tuple(DatabaseOrder(field) for field in group_by)
        for term in reversed(terms):
            results.sort(
                key=lambda item, field=term.field: self._sort_value(
                    item.count if field == "count" else item.values.get(field)
                ),
                reverse=term.descending,
            )
        return results
