# SPDX-License-Identifier: GPL-2.0

import argparse
import datetime
import os
import pathlib
import re
import sys
import tempfile
import unittest

try:
    import sqlalchemy
except ImportError:
    sqlalchemy = None


UTIL_DIR = pathlib.Path(__file__).resolve().parents[1] / "util"
sys.path.insert(0, str(UTIL_DIR))

if sqlalchemy is not None:
    from ras_db import RasDatabase, RasDatabaseCommand  # noqa: E402


class RasDatabaseTests:
    """Tests shared by the SQLite, MySQL and PostgreSQL groups."""

    backend = None
    DATABASE_SOURCES = (
        "db/ras-record.c",
        "events-arch-arm/non-standard-ampere.c",
        "events-arch-arm/non-standard-hisi_hip08.c",
        "events-arch-arm/non-standard-hisilicon.c",
        "events-arch-arm/non-standard-jaguarmicro.c",
        "events-arch-arm/non-standard-nvidia.c",
        "events-arch-arm/non-standard-yitian.c",
    )

    @classmethod
    def _descriptors(cls):
        root = pathlib.Path(__file__).resolve().parents[1]
        descriptors = {}
        for filename in cls.DATABASE_SOURCES:
            source = (root / filename).read_text(encoding="utf-8")
            field_arrays = dict(re.findall(
                r"static const struct db_fields\s+(\w+)\[\]\s*=\s*"
                r"\{(.*?)\n\};", source, re.DOTALL
            ))
            for body in re.findall(
                r"(?:static\s+)?const struct db_table_descriptor\s+\w+\s*="
                r"\s*\{(.*?)\n\};", source, re.DOTALL
            ):
                name = re.search(r'\.name\s*=\s*"([^"]+)"', body)
                fields = re.search(r"\.fields\s*=\s*(\w+)", body)
                if not name or not fields or fields.group(1) not in field_arrays:
                    continue
                descriptors[name.group(1)] = re.findall(
                    r'\.name\s*=\s*"([^"]+)".*?'
                    r'\.type\s*=\s*(DB_TYPE_\w+)',
                    field_arrays[fields.group(1)], re.DOTALL,
                )
        return descriptors

    def _database_options(self):
        if self.backend == "mysql":
            return {"mysql_conn_parms": {
                "host": os.environ.get("RAS_MYSQL_HOST", ""),
                "port": os.environ.get("RAS_MYSQL_PORT", "3306"),
                "user": os.environ.get("RAS_MYSQL_USER", "rasdaemon"),
                "password": os.environ.get("RAS_MYSQL_PASSWORD", "mypass"),
                "database": os.environ.get(
                    "RAS_MYSQL_DATABASE", "rasdaemon_test"
                ),
                "socket": os.environ.get("RAS_MYSQL_SOCKET", ""),
            }}
        if self.backend == "postgresql":
            return {"postgresql_conn_parms": {
                "host": os.environ.get("RAS_PG_HOST", "127.0.0.1"),
                "port": os.environ.get("RAS_PG_PORT", "5432"),
                "user": os.environ.get("RAS_PG_USER", "rasdaemon"),
                "password": os.environ.get("RAS_PG_PASSWORD", "mypass"),
                "database": os.environ.get("RAS_PG_DATABASE", "rasdaemon_test"),
                "schema": os.environ.get("RAS_PG_SCHEMA", "rasdaemon"),
            }}

        self.temporary_directory = tempfile.TemporaryDirectory()
        return {
            "sqlite3_database": os.path.join(
                self.temporary_directory.name, "events.db"
            ),
        }

    def setUp(self):
        self.descriptors = self._descriptors()
        try:
            self.database = RasDatabase(
                self.backend, hostname="local-host", **self._database_options()
            )
            with self.database.engine.connect() as connection:
                connection.exec_driver_sql("SELECT 1")
        except (ImportError, ModuleNotFoundError) as error:
            self.skipTest(f"{self.backend} SQLAlchemy driver unavailable: {error}")

        self.engine = self.database.engine
        existing = sqlalchemy.MetaData(schema=self.database.schema)
        existing.reflect(bind=self.engine)
        existing.drop_all(self.engine)
        self.metadata = sqlalchemy.MetaData(schema=self.database.schema)
        self._define_tables()
        self.metadata.create_all(self.engine)
        self._insert_records()

    def tearDown(self):
        if not hasattr(self, "database"):
            return
        self.metadata.drop_all(self.engine, checkfirst=True)
        self.database.close()
        if hasattr(self, "temporary_directory"):
            self.temporary_directory.cleanup()

    def _define_tables(self):
        type_map = {
            "DB_TYPE_INT32": sqlalchemy.Integer,
            "DB_TYPE_INT64": sqlalchemy.BigInteger,
            "DB_TYPE_TIMESTAMP": (
                sqlalchemy.Text if self.backend == "sqlite3"
                else sqlalchemy.DateTime(timezone=self.backend == "postgresql")
            ),
            "DB_TYPE_TEXT": sqlalchemy.Text,
            "DB_TYPE_BLOB": sqlalchemy.LargeBinary,
        }
        for table_name, fields in self.descriptors.items():
            columns = []
            for field_name, field_type in fields:
                if field_type == "DB_TYPE_SERIAL":
                    columns.append(sqlalchemy.Column(
                        field_name, sqlalchemy.BigInteger().with_variant(
                            sqlalchemy.Integer, "sqlite"
                        ), primary_key=True, autoincrement=True,
                    ))
                else:
                    columns.append(sqlalchemy.Column(
                        field_name, type_map[field_type]
                    ))
            if self.backend != "sqlite3":
                hostname_type = (sqlalchemy.String(255)
                                 if self.backend == "mysql"
                                 else sqlalchemy.Text)
                columns.insert(1, sqlalchemy.Column("hostname", hostname_type))
            sqlalchemy.Table(table_name, self.metadata, *columns)

    def _insert_records(self):
        with self.engine.begin() as connection:
            for table in self.metadata.tables.values():
                records = []
                for row in range(10):
                    record = {}
                    for column in table.columns:
                        if column.primary_key:
                            continue
                        if column.name == "hostname":
                            value = "local-host" if row < 5 else "remote-a"
                        elif column.name == "timestamp":
                            if self.backend == "sqlite3":
                                value = f"2026-03-{row + 1:02d} 10:00:00"
                            else:
                                value = datetime.datetime(
                                    2026, 3, row + 1, 10, 0,
                                    tzinfo=datetime.timezone.utc,
                                )
                        elif isinstance(column.type, sqlalchemy.LargeBinary):
                            value = bytes((row, 0x52, 0x41, 0x53))
                        elif isinstance(column.type, sqlalchemy.Text):
                            value = f"{table.name}-{column.name}-{row}"
                        else:
                            value = row + 1
                        record[column.name] = value
                    records.append(record)
                connection.execute(table.insert(), records)

    def test_discovers_event_tables_and_creates_indexes(self):
        tables = self.database.discover_tables()

        self.assertEqual(list(tables), sorted(self.descriptors))
        created = self.database.create_missing_indexes()
        columns_per_table = 1 if self.backend == "sqlite3" else 2
        self.assertEqual(len(created), columns_per_table * len(self.descriptors))
        self.assertEqual(self.database.create_missing_indexes(), [])

        inspector = sqlalchemy.inspect(self.engine)
        expected = {("timestamp",)}
        if self.backend != "sqlite3":
            expected.add(("hostname",))
        for table_name in self.descriptors:
            indexes = inspector.get_indexes(
                table_name, schema=self.database.schema
            )
            self.assertEqual(
                {tuple(index["column_names"]) for index in indexes}, expected
            )

    def test_groups_all_tables_by_hostname_and_orders_by_timestamp(self):
        groups = self.database.records()

        expected_hosts = (["local-host"] if self.backend == "sqlite3"
                          else ["local-host", "remote-a"])
        self.assertEqual(list(groups), expected_hosts)
        expected_count = 10 if self.backend == "sqlite3" else 5
        for hostname in expected_hosts:
            self.assertEqual(
                len(groups[hostname]), expected_count * len(self.descriptors)
            )
            timestamps = [event.timestamp for event in groups[hostname]]
            self.assertEqual(
                [str(value) for value in timestamps],
                sorted(str(value) for value in timestamps),
            )

    def test_filters_records_by_inclusive_date_range(self):
        groups = self.database.records(
            since="2026-03-03 10:00:00", until="2026-03-05 10:00:00"
        )

        self.assertEqual(list(groups), ["local-host"])
        self.assertEqual(len(groups["local-host"]), 3 * len(self.descriptors))

    def test_hostname_filter(self):
        groups = self.database.records(hostname="remote-a")

        if self.backend == "sqlite3":
            self.assertEqual(list(groups), ["local-host"])
            self.assertEqual(
                len(groups["local-host"]), 10 * len(self.descriptors)
            )
        else:
            self.assertEqual(list(groups), ["remote-a"])
            self.assertEqual(
                len(groups["remote-a"]), 5 * len(self.descriptors)
            )

    def test_database_command_options_depend_on_backend(self):
        parser = argparse.ArgumentParser()
        subparsers = parser.add_subparsers()
        RasDatabaseCommand("ras-mc-ctl", subparsers)
        command = ["database", "--since", "2026-03-01",
                   "--until", "2026-03-02"]
        if self.backend != "sqlite3":
            command.extend(["--hostname", "remote-a"])
        args = parser.parse_args(command)
        self.assertEqual(args.since, "2026-03-01")
        self.assertEqual(args.until, "2026-03-02")
        expected_hostname = None if self.backend == "sqlite3" else "remote-a"
        self.assertEqual(args.hostname, expected_hostname)


@unittest.skipIf(sqlalchemy is None, "SQLAlchemy is not installed")
class SqliteRasDatabaseTest(RasDatabaseTests, unittest.TestCase):
    backend = "sqlite3"

    def test_database_url_uses_complete_path(self):
        path = os.path.join(self.temporary_directory.name, "complete-path.db")
        url = RasDatabase._database_url(
            "sqlite3", {"sqlite3_database": path}
        )

        self.assertEqual(url.database, path)


@unittest.skipIf(sqlalchemy is None, "SQLAlchemy is not installed")
class MysqlRasDatabaseTest(RasDatabaseTests, unittest.TestCase):
    backend = "mysql"


@unittest.skipIf(sqlalchemy is None, "SQLAlchemy is not installed")
class PostgresqlRasDatabaseTest(RasDatabaseTests, unittest.TestCase):
    backend = "postgresql"


if __name__ == "__main__":
    unittest.main()
