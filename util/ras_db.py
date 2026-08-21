# SPDX-License-Identifier: GPL-2.0

"""Discover and display rasdaemon events stored in SQL databases."""

from __future__ import annotations

import collections
import datetime
import hashlib
import os
import re
import socket
from dataclasses import dataclass
from typing import Any, Iterable, Mapping

from sqlalchemy import Index, MetaData, Table, create_engine, inspect, select
from sqlalchemy.engine import Engine, URL


SUPPORTED_BACKENDS = ("sqlite3", "mysql", "postgresql")


@dataclass(frozen=True)
class DatabaseEvent:
    """One record read from a reflected rasdaemon event table."""

    table: str
    hostname: str
    timestamp: Any
    values: Mapping[str, Any]


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
            engine = create_engine(self._database_url(db_backend, kwargs))
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
            query = {}
            unix_socket = cls._get(params, "socket", "")
            if unix_socket:
                query["unix_socket"] = unix_socket
            if str(cls._get(params, "use_ssl", "false")).lower() in (
                    "1", "true", "yes", "on"):
                query["ssl"] = "true"
            return URL.create(
                "mysql+mysqldb",
                username=cls._get(params, "user", "rasdaemon"),
                password=cls._get(params, "password", ""),
                host=None if unix_socket else cls._get(params, "host", "localhost"),
                port=int(cls._get(params, "port", 3306)),
                database=cls._get(params, "database", "rasdaemon"),
                query=query,
            )

        query = {"connect_timeout": str(cls._get(params, "connect_timeout", 10))}
        ssl_mode = cls._get(params, "ssl_mode", "false")
        if ssl_mode and ssl_mode != "false":
            query["sslmode"] = str(ssl_mode)
        return URL.create(
            "postgresql+psycopg2",
            username=cls._get(params, "user", "rasdaemon"),
            password=cls._get(params, "password", ""),
            host=cls._get(params, "host", "localhost"),
            port=int(cls._get(params, "port", 5432)),
            database=cls._get(params, "database", "rasdaemon"),
            query=query,
        )

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

    @staticmethod
    def _index_name(table: str, column: str, maximum: int) -> str:
        raw = re.sub(r"[^A-Za-z0-9_]", "_", f"idx_{table}_{column}")
        if len(raw) <= maximum:
            return raw
        digest = hashlib.sha1(raw.encode("utf-8")).hexdigest()[:8]
        return f"{raw[:maximum - len(digest) - 1]}_{digest}"

    def create_missing_indexes(self) -> list[str]:
        """Create useful missing indexes and return their names.

        Timestamp indexes apply to every backend.  Hostname indexes only apply
        to remote databases, whose schemas carry records from several hosts.
        """

        tables = self.tables or self.discover_tables()
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

    @staticmethod
    def _sort_timestamp(value: Any) -> tuple[int, Any]:
        if value is None:
            return (1, "")
        if isinstance(value, (datetime.date, datetime.datetime)):
            return (0, value.isoformat())
        return (0, str(value))

    def records(self, since: str | None = None, until: str | None = None,
                hostname: str | None = None) -> dict[str, list[DatabaseEvent]]:
        """Return records grouped by hostname and ordered by timestamp."""

        tables = self.tables or self.discover_tables()
        grouped: dict[str, list[DatabaseEvent]] = collections.defaultdict(list)
        with self.engine.connect() as connection:
            for table_name, table in tables.items():
                if (hostname and self.db_backend != "sqlite3"
                        and "hostname" not in table.c):
                    continue
                statement = select(table)
                if since:
                    statement = statement.where(table.c.timestamp >= since)
                if until:
                    statement = statement.where(table.c.timestamp <= until)
                if hostname and self.db_backend != "sqlite3":
                    statement = statement.where(table.c.hostname == hostname)
                for row in connection.execute(statement).mappings():
                    values = dict(row)
                    event_hostname = values.get("hostname") or self.hostname
                    grouped[str(event_hostname)].append(DatabaseEvent(
                        table_name, str(event_hostname), values.get("timestamp"),
                        values
                    ))

        for events in grouped.values():
            events.sort(key=lambda event: (
                self._sort_timestamp(event.timestamp), event.table,
                str(event.values.get("id", "")),
            ))
        return dict(sorted(grouped.items()))

    @staticmethod
    def format_records(groups: Mapping[str, Iterable[DatabaseEvent]]) -> str:
        """Format discovered records without relying on fixed table schemas."""

        output = []
        for hostname, events in groups.items():
            output.append(f"Hostname: {hostname}")
            for event in events:
                fields = (
                    f"{key}={value}" for key, value in event.values.items()
                    if key not in ("hostname", "timestamp")
                )
                output.append(
                    f"  {event.timestamp} {event.table}: {', '.join(fields)}"
                )
        return "\n".join(output) + ("\n" if output else "")

    def close(self) -> None:
        """Release the engine's connection pool."""

        self.engine.dispose()


class RasDatabaseCommand:
    """Register and implement the ras-mc-ctl database command."""

    def __init__(self, prog: str, subparsers: Any) -> None:
        self.prog = prog
        parser = subparsers.add_parser(
            "database", aliases=["db"],
            help="Display records from the configured rasdaemon database.",
        )
        parser.add_argument(
            "--indexes-only", action="store_true",
            help="Create missing indexes without reading event records.",
        )
        parser.add_argument(
            "--since", metavar="YYYY-MM-DD",
            help="Only display records at or after this date.",
        )
        parser.add_argument(
            "--until", metavar="YYYY-MM-DD",
            help="Only display records at or before this date.",
        )
        parser.add_argument(
            "--hostname",
            help="Only display records stored for this hostname (ignored with sqlite3).",
        )

        parser.set_defaults(func=self.run)

    def run(self, config:Any, args: Any) -> None:
        database = RasDatabase.from_config(config)
        try:
            created = database.create_missing_indexes()
            for name in created:
                print(f"Created index {name}")
            if args.indexes_only:
                return
            print(database.format_records(database.records(
                since=args.since, until=args.until, hostname=args.hostname
            )), end="")
        finally:
            database.close()
