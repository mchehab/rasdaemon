# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

"""Discover and display rasdaemon events stored in SQL databases."""

from __future__ import annotations

import argparse
import base64
import collections
import datetime
import fnmatch
import hashlib
import json
import logging
import os
import re
import socket
import textwrap
from typing import Any, Iterable, Mapping

from sqlalchemy import Index, MetaData, Table, create_engine, event, inspect
from sqlalchemy.engine import Engine, URL

from ras_filter import (DatabaseCount, DatabaseEvent, DatabaseFilter,
                        DatabaseOrder, RasDatabaseQuery)

SUPPORTED_BACKENDS = ("sqlite3", "mysql", "postgresql")
logger = logging.getLogger(__name__)


def _decode_sqlite_text(value: bytes) -> str:
    """Decode SQLite TEXT while preserving malformed bytes visibly."""

    return value.decode("utf-8", errors="backslashreplace")


class RasDatabase:
    """SQLAlchemy-backed access to rasdaemon event databases."""

    def __init__(self, db_backend: str, *, engine: Engine | None = None,
                 hostname: str = "", **kwargs: Any) -> None:
        if db_backend not in SUPPORTED_BACKENDS:
            raise ValueError(f"unsupported database backend: {db_backend}")

        self.db_backend = db_backend
        self.hostname = hostname or socket.gethostname()
        self.schema = None
        if engine is None:
            connect_args = self._database_connect_args(db_backend, kwargs)
            engine = create_engine(
                self._database_url(db_backend, kwargs),
                connect_args=connect_args,
            )
            if db_backend == "mysql" and "ssl" in connect_args:
                @event.listens_for(engine, "connect")
                def require_mysql_tls(dbapi_connection: Any,
                                      _connection: Any) -> None:
                    self._require_mysql_tls(dbapi_connection)
        if db_backend == "sqlite3":
            @event.listens_for(engine, "connect")
            def set_text_factory(dbapi_connection: Any, _connection: Any) -> None:
                dbapi_connection.text_factory = _decode_sqlite_text
        self.engine = engine
        if db_backend == "postgresql":
            params = kwargs.get("postgresql_conn_parms", {})
            self.schema = self._get(params, "schema", "rasdaemon")

        self.metadata = MetaData(schema=self.schema)
        self.tables: dict[str, Table] = {}

    @staticmethod
    def _get(values: Any, name: str, default: Any = None) -> Any:
        if isinstance(values, Mapping):
            return values.get(name, default)
        return getattr(values, name, default)

    @classmethod
    def from_config(cls, config: Any, *, engine: Engine | None = None):
        """Build a connection from :class:`RasdaemonConfig`."""

        sqlite = config.sqlite3_conn_parms
        return cls(
            config.db_backend,
            engine=engine,
            sqlite3_database=cls._get(sqlite, "database", "ras-mc_event.db"),
            mysql_conn_parms=config.mysql_conn_parms,
            postgresql_conn_parms=config.pg_conn_parms,
        )

    @classmethod
    def _database_url(cls, backend: str, kwargs: Mapping[str, Any]) -> URL:
        if backend == "sqlite3":
            path = kwargs.get("sqlite3_database", "ras-mc_event.db")
            return URL.create("sqlite", database=os.path.abspath(path))

        key = "mysql_conn_parms" if backend == "mysql" else "postgresql_conn_parms"
        params = kwargs.get(key, {})
        if backend == "mysql":
            query = {
                "connect_timeout": str(
                    cls._get(params, "connect_timeout", 10)
                ),
            }
            unix_socket = cls._get(params, "socket", "")
            if unix_socket:
                query["unix_socket"] = unix_socket
            return URL.create(
                "mysql+mysqldb",
                username=cls._get(params, "user", "rasdaemon"),
                password=cls._get(params, "password", ""),
                host=None if unix_socket else cls._get(params, "host", "") or None,
                port=int(cls._get(params, "port", 3306)),
                database=cls._get(params, "database", "rasdaemon"),
                query=query,
            )

        query = {"connect_timeout": str(cls._get(params, "connect_timeout", 10))}
        ssl_mode = str(cls._get(params, "ssl_mode", "") or "")
        if ssl_mode and ssl_mode.lower() != "false":
            query["sslmode"] = ssl_mode
        elif str(cls._get(params, "use_ssl", "false")).lower() in (
                "1", "true", "yes", "on"):
            query["sslmode"] = "require"
        return URL.create(
            "postgresql+psycopg2",
            username=cls._get(params, "user", "rasdaemon"),
            password=cls._get(params, "password", ""),
            host=cls._get(params, "host", "") or None,
            port=int(cls._get(params, "port", 5432)),
            database=cls._get(params, "database", "rasdaemon"),
            query=query,
        )

    @classmethod
    def _database_connect_args(cls, backend: str,
                               kwargs: Mapping[str, Any]) -> dict[str, Any]:
        """Return DBAPI connection arguments not representable in a URL."""

        if backend != "mysql":
            return {}

        params = kwargs.get("mysql_conn_parms", {})
        if str(cls._get(params, "use_ssl", "false")).lower() not in (
                "1", "true", "yes", "on"):
            return {}

        # mysqlclient accepts SSL parameters only as a mapping.  The connect
        # listener above rejects a connection that did not actually negotiate
        # TLS, matching the daemon's required-TLS policy.
        return {"ssl": {}}

    @staticmethod
    def _require_mysql_tls(dbapi_connection: Any) -> None:
        """Reject a MySQL connection that did not negotiate TLS."""

        cursor = dbapi_connection.cursor()
        try:
            cursor.execute("SHOW STATUS LIKE 'Ssl_cipher'")
            row = cursor.fetchone()
        finally:
            cursor.close()
        if not row or not row[1]:
            raise RuntimeError("MySQL TLS was required but is not in use")

    def discover_tables(self) -> dict[str, Table]:
        """Reflect all tables containing a timestamp column."""

        inspector = inspect(self.engine)
        names = inspector.get_table_names(schema=self.schema)
        discovered = {}
        for name in sorted(names):
            table = Table(
                name, self.metadata, schema=self.schema,
                autoload_with=self.engine, extend_existing=True,
            )
            if "timestamp" in table.c:
                discovered[name] = table
        self.tables = discovered
        return dict(discovered)

    def select_tables(self, include: Iterable[str] | None = None,
                      exclude: Iterable[str] | None = None
                      ) -> dict[str, Table]:
        """Select discovered tables using exact names or shell-style globs."""

        tables = self.tables or self.discover_tables()
        includes = list(include or ())
        excludes = list(exclude or ())
        selected: set[str] = set()

        if not includes:
            selected.update(tables)
        for pattern in includes:
            matches = {
                name for name in tables if fnmatch.fnmatchcase(name, pattern)
            }
            if not matches:
                if not any(character in pattern for character in "*?["):
                    raise ValueError(f"unknown event table: {pattern}")
                logger.debug("Table pattern matched no event tables: %s", pattern)
            selected.update(matches)

        for pattern in excludes:
            matches = {
                name for name in tables if fnmatch.fnmatchcase(name, pattern)
            }
            if not matches:
                if not any(character in pattern for character in "*?["):
                    raise ValueError(f"unknown event table: {pattern}")
                logger.debug("Table exclusion matched no event tables: %s", pattern)
            selected.difference_update(matches)

        return {name: tables[name] for name in sorted(selected)}

    @staticmethod
    def _index_name(table: str, column: str, maximum: int) -> str:
        raw = re.sub(r"[^A-Za-z0-9_]", "_", f"idx_{table}_{column}")
        if len(raw) <= maximum:
            return raw
        digest = hashlib.sha1(raw.encode("utf-8")).hexdigest()[:8]
        return f"{raw[:maximum - len(digest) - 1]}_{digest}"

    def create_missing_indexes(self, tables: Mapping[str, Table] | None = None
                               ) -> list[str]:
        """Create useful missing indexes and return their names.

        Timestamp indexes apply to every backend.  Hostname indexes only apply
        to remote databases, whose schemas carry records from several hosts.
        """

        tables = tables if tables is not None else (
            self.tables or self.discover_tables()
        )
        inspector = inspect(self.engine)
        maximum = self.engine.dialect.max_identifier_length or 63
        created = []
        for name, table in tables.items():
            columns = ["timestamp"]
            if self.db_backend in ("mysql", "postgresql") and "hostname" in table.c:
                columns.append("hostname")
            existing = {
                tuple(item.get("column_names") or ())
                for item in inspector.get_indexes(name, schema=self.schema)
            }
            for column in columns:
                if (column,) in existing:
                    continue
                index_name = self._index_name(name, column, maximum)
                Index(index_name, table.c[column]).create(
                    self.engine, checkfirst=True
                )
                created.append(index_name)
        return created

    def _query(self, tables: Mapping[str, Table]) -> RasDatabaseQuery:
        """Create a dynamic query for the selected reflected tables."""

        return RasDatabaseQuery(self.db_backend, self.hostname, tables, logger)

    def records(self, since: str | None = None, until: str | None = None,
                hostname: str | None = None,
                tables: Mapping[str, Table] | None = None,
                filters: Iterable[DatabaseFilter] = (),
                severity: str | None = None,
                select_fields: Iterable[str] = (),
                order_by: Iterable[DatabaseOrder] = (),
                ) -> dict[str, list[DatabaseEvent]]:
        """Return records grouped by hostname and ordered by the query."""

        tables = tables if tables is not None else (
            self.tables or self.discover_tables()
        )
        with self.engine.connect() as connection:
            return self._query(tables).records(
                connection, since=since, until=until, hostname=hostname,
                filters=filters, severity=severity, select_fields=select_fields,
                order_by=order_by,
            )

    def counts(self, since: str | None = None, until: str | None = None,
               hostname: str | None = None,
               tables: Mapping[str, Table] | None = None,
               filters: Iterable[DatabaseFilter] = (),
               severity: str | None = None,
               group_by: Iterable[str] = (),
               order_by: Iterable[DatabaseOrder] = ()
               ) -> list[DatabaseCount]:
        """Count matching events, optionally grouped by runtime fields."""

        tables = tables if tables is not None else (
            self.tables or self.discover_tables()
        )
        with self.engine.connect() as connection:
            return self._query(tables).counts(
                connection, since=since, until=until, hostname=hostname,
                filters=filters, severity=severity, group_by=group_by,
                order_by=order_by,
            )

    def summary(self, since: str | None = None, until: str | None = None,
                hostname: str | None = None,
                tables: Mapping[str, Table] | None = None,
                filters: Iterable[DatabaseFilter] = (),
                severity: str | None = None,
                ) -> dict[str, dict[str, int]]:
        """Return event counts grouped by hostname and table."""

        grouped: dict[str, dict[str, int]] = collections.defaultdict(dict)
        for result in self.counts(
                since=since, until=until, hostname=hostname, tables=tables,
                filters=filters, severity=severity,
                group_by=("hostname", "table")):
            if result.count:
                grouped[str(result.values["hostname"])][
                    str(result.values["table"])
                ] = result.count

        return {
            host: dict(sorted(counts.items()))
            for host, counts in sorted(grouped.items())
        }

    @staticmethod
    def format_records(groups: Mapping[str, Iterable[DatabaseEvent]],
                       select_fields: Iterable[str] = ()) -> str:
        """Format discovered records without relying on fixed table schemas."""

        select_fields = tuple(select_fields)
        output = []
        for hostname, events in groups.items():
            output.append(f"Hostname: {hostname}")
            for event in events:
                if select_fields:
                    fields = (
                        f"{field}={RasDatabaseQuery.event_value(event, field)}"
                        for field in select_fields
                        if field not in ("hostname", "timestamp", "table")
                    )
                else:
                    fields = (
                        f"{key}={value}" for key, value in event.values.items()
                        if key not in ("hostname", "timestamp")
                    )
                output.append(
                    f"  {event.timestamp} {event.table}: {', '.join(fields)}"
                )
        return "\n".join(output) + ("\n" if output else "")

    @staticmethod
    def format_counts(counts: Iterable[DatabaseCount],
                      group_by: Iterable[str] = ()) -> str:
        """Format a total or grouped event counts."""

        counts = list(counts)
        group_by = tuple(group_by)
        if not group_by:
            count = counts[0].count if counts else 0
            return f"Count: {count}\n"
        output = []
        for item in counts:
            fields = ", ".join(
                f"{field}={item.values[field]}" for field in group_by
            )
            output.append(f"{fields}: {item.count} event(s)")
        return "\n".join(output) + ("\n" if output else "")

    @staticmethod
    def format_summary(groups: Mapping[str, Mapping[str, int]]) -> str:
        """Format event counts grouped by hostname and table."""

        output = []
        for hostname, counts in groups.items():
            output.append(f"Hostname: {hostname}")
            for table, count in counts.items():
                output.append(f"  {table}: {count} event(s)")
        return "\n".join(output) + ("\n" if output else "")

    @staticmethod
    def _json_default(value: Any) -> Any:
        """Convert database scalar types which JSON cannot encode directly."""

        if isinstance(value, (bytes, bytearray, memoryview)):
            return {
                "encoding": "base64",
                "data": base64.b64encode(bytes(value)).decode("ascii"),
            }
        if isinstance(value, (datetime.date, datetime.datetime,
                              datetime.time)):
            return value.isoformat()
        # Some database drivers return Decimal or backend-specific scalar
        # classes.  Their string representation is lossless and portable.
        return str(value)

    @classmethod
    def format_json(cls, mode: str, data: Any) -> str:
        """Format one versioned, machine-readable database result."""

        document = {"format_version": 1, "mode": mode}
        document.update(data)
        return json.dumps(
            document, default=cls._json_default, ensure_ascii=False,
            sort_keys=True,
        ) + "\n"

    @classmethod
    def format_records_json(
            cls, groups: Mapping[str, Iterable[DatabaseEvent]],
            select_fields: Iterable[str] = ()) -> str:
        """Format detailed records as a versioned JSON document."""

        select_fields = tuple(select_fields)
        records = []
        for hostname, events in groups.items():
            for event in events:
                if select_fields:
                    fields = {
                        field: RasDatabaseQuery.event_value(event, field)
                        for field in select_fields
                        if field not in ("hostname", "timestamp", "table")
                    }
                else:
                    fields = {
                        key: value for key, value in event.values.items()
                        if key not in ("hostname", "timestamp")
                    }
                records.append({
                    "hostname": hostname,
                    "timestamp": event.timestamp,
                    "table": event.table,
                    "fields": fields,
                })
        return cls.format_json("errors", {"records": records})

    @classmethod
    def format_counts_json(cls, mode: str,
                           counts: Iterable[DatabaseCount]) -> str:
        """Format aggregate database results as a versioned JSON document."""

        return cls.format_json(mode, {"groups": [
            {"values": dict(item.values), "count": item.count}
            for item in counts
        ]})

    def close(self) -> None:
        """Release the engine's connection pool."""

        self.engine.dispose()


class RasDatabaseCommand:
    """Register and implement the ras-mc-ctl database command."""

    def __init__(self, prog: str, subparsers: Any) -> None:
        self.prog = prog
        parser = subparsers.add_parser(
            "database", aliases=["db"],
            description=(
                "Display and summarize RAS events from the configured "
                "database. Table selectors accept exact names or "
                "shell-style patterns."
            ),
            help="Display records from the configured rasdaemon database.",
            formatter_class=argparse.RawDescriptionHelpFormatter,
            epilog=textwrap.dedent("""\
                Examples:
                  # List tables which currently contain errors.
                  ras-mc-ctl database -E

                  # Count corrected EDAC events.
                  ras-mc-ctl database --count --table mc_event --corrected

                  # Show the newest PCIe AER events first.
                  ras-mc-ctl database --errors --table aer_event \\
                      --order-by timestamp:desc

                  # Count corrected EDAC events by DIMM label.
                  ras-mc-ctl database --count --table mc_event --corrected \\
                      --group-by label --order-by count:desc

                  # Summarize HiSilicon OEM records by severity.
                  ras-mc-ctl database --count --table 'hip08_*' \\
                      --group-by err_severity --order-by count:desc
            """),
        )
        self.parser = parser
        output = parser.add_mutually_exclusive_group()
        output.add_argument(
            "--errors", "-e", action="store_true",
            help="Display detailed error records (the default).",
        )
        output.add_argument(
            "--summary", "-S", action="store_true",
            help="Display event counts grouped by hostname and table.",
        )
        output.add_argument(
            "--count", "-C", action="store_true",
            help="Count matching events, optionally grouped by --group-by.",
        )
        output.add_argument(
            "--errors-per-table", "-E", action="store_true",
            help="Count matching errors in each non-empty event table.",
        )
        output.add_argument(
            "--list-tables", "-L", action="store_true",
            help="List discovered event tables and exit.",
        )
        output.add_argument(
            "--describe", "-D", action="store_true",
            help="Describe fields and types for selected event tables and exit.",
        )
        output.add_argument(
            "--create-index", "-I", action="store_true",
            help="Create missing indexes for the selected event tables and exit.",
        )
        parser.add_argument(
            "--since", "-s", metavar="YYYY-MM-DD",
            help="Only display records at or after this date.",
        )
        parser.add_argument(
            "--until", "-u", type=datetime.date.fromisoformat,
            metavar="YYYY-MM-DD",
            help="Only display records at or before this date.",
        )
        parser.add_argument(
            "--hostname", "-H", metavar="HOSTNAME",
            help="Only display records for this hostname (ignored with SQLite).",
        )
        parser.add_argument(
            "--where", "-w", action="append", default=[],
            metavar="FIELD[OP]VALUE",
            help="Require a field comparison; may be repeated.",
        )
        parser.add_argument(
            "--select", "-x", dest="select_fields", action="append", default=[],
            metavar="FIELD", help="Display only this field in detailed output.",
        )
        parser.add_argument(
            "--group-by", "-g", action="append", default=[], metavar="FIELD",
            help="Group --count output by this field; may be repeated.",
        )
        parser.add_argument(
            "--order-by", "-o", action="append", default=[],
            metavar="FIELD[:asc|desc]",
            help="Order detailed output or --count groups; may be repeated.",
        )
        severity = parser.add_mutually_exclusive_group()
        severity.add_argument(
            "--corrected", "-c", dest="severity", action="store_const",
            const="corrected", help="Select corrected errors.",
        )
        severity.add_argument(
            "--uncorrected", "-U", dest="severity", action="store_const",
            const="uncorrected", help="Select uncorrected errors.",
        )
        severity.add_argument(
            "--deferred", "-d", dest="severity", action="store_const",
            const="deferred", help="Select deferred errors.",
        )
        severity.add_argument(
            "--fatal", "-f", dest="severity", action="store_const",
            const="fatal", help="Select fatal errors.",
        )
        severity.add_argument(
            "--info", "-i", dest="severity", action="store_const",
            const="info", help="Select informational errors.",
        )
        severity.add_argument(
            "--recoverable", "-r", dest="severity", action="store_const",
            const="recoverable", help="Select recoverable errors.",
        )
        parser.add_argument(
            "--table", "-t", action="append", default=[], metavar="PATTERN",
            help="Include an exact table or shell-style pattern (repeatable).",
        )
        parser.add_argument(
            "--except", "-X", dest="exclude_table", action="append", default=[],
            metavar="PATTERN",
            help="Exclude an exact table or shell-style pattern (repeatable).",
        )
        parser.add_argument(
            "--verbose", "-v", action="count", default=argparse.SUPPRESS,
            help="Describe tables selected for this query.",
        )
        parser.add_argument(
            "--json", action="store_true",
            help="Write a versioned JSON document instead of text output.",
        )
        parser.set_defaults(func=self.run)

    def run(self, config: Any, args: Any) -> None:
        database = RasDatabase.from_config(config)
        until = None
        if args.until:
            until = "{} 00:00:00".format(
                args.until + datetime.timedelta(days=1)
            )
        try:
            try:
                tables = database.select_tables(args.table, args.exclude_table)
                filters = RasDatabaseQuery.parse_filters(args.where)
                ordering = RasDatabaseQuery.parse_ordering(args.order_by)
                select_fields = tuple(
                    RasDatabaseQuery.validate_field(field)
                    for field in args.select_fields
                )
                group_by = tuple(
                    RasDatabaseQuery.validate_field(field) for field in args.group_by
                )
            except ValueError as error:
                self.parser.error(str(error))
            if args.errors_per_table and args.group_by:
                self.parser.error("--errors-per-table has a fixed table grouping")
            if args.group_by and not args.count:
                self.parser.error("--group-by requires --count")
            if args.select_fields and (
                    args.count or args.summary or args.errors_per_table):
                self.parser.error("--select is only available with detailed errors")
            if args.summary and (args.group_by or args.order_by):
                self.parser.error("--summary has a fixed hostname/table grouping")
            if args.list_tables:
                if args.json:
                    print(database.format_json(
                        "list-tables", {"tables": list(tables)}
                    ), end="")
                else:
                    for name in tables:
                        print(name)
                return
            if args.describe:
                if args.json:
                    description = {
                        name: [
                            {"name": column.name, "type": str(column.type)}
                            for column in table.columns
                        ] for name, table in tables.items()
                    }
                    print(database.format_json(
                        "describe", {"tables": description}
                    ), end="")
                else:
                    for name, table in tables.items():
                        print(f"{name}:")
                        for column in table.columns:
                            print(f"  {column.name}: {column.type}")
                return
            if args.create_index:
                created = database.create_missing_indexes(tables)
                if args.json:
                    print(database.format_json(
                        "create-index", {"created_indexes": created}
                    ), end="")
                else:
                    for name in created:
                        print(f"Created index {name}")
                return
            if args.summary:
                result = database.summary(
                    since=args.since, until=until,
                    hostname=args.hostname, tables=tables, filters=filters,
                    severity=args.severity,
                )
                if args.json:
                    print(database.format_json(
                        "summary", {"hosts": result}
                    ), end="")
                else:
                    print(database.format_summary(result), end="")
            elif args.count:
                result = database.counts(
                    since=args.since, until=until, hostname=args.hostname,
                    tables=tables, filters=filters, severity=args.severity,
                    group_by=group_by, order_by=ordering,
                )
                if args.json:
                    print(database.format_counts_json("count", result), end="")
                else:
                    print(database.format_counts(result, group_by), end="")
            elif args.errors_per_table:
                group_by = ("table",)
                result = database.counts(
                    since=args.since, until=until, hostname=args.hostname,
                    tables=tables, filters=filters, severity=args.severity,
                    group_by=group_by, order_by=ordering,
                )
                if args.json:
                    print(database.format_counts_json(
                        "errors-per-table", result
                    ), end="")
                else:
                    print(database.format_counts(result, group_by), end="")
            else:
                result = database.records(
                    since=args.since, until=until,
                    hostname=args.hostname, tables=tables, filters=filters,
                    severity=args.severity, select_fields=select_fields,
                    order_by=ordering,
                )
                if args.json:
                    print(database.format_records_json(
                        result, select_fields
                    ), end="")
                else:
                    print(database.format_records(result, select_fields), end="")
        finally:
            database.close()
