# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

import argparse
import contextlib
import datetime
import io
import json
import os
import pathlib
import re
import sys
import tempfile
import time
import unittest
from unittest import mock

try:
    import sqlalchemy
except ImportError:
    sqlalchemy = None


UTIL_DIR = pathlib.Path(__file__).resolve().parents[1] / "util"
sys.path.insert(0, str(UTIL_DIR))

if sqlalchemy is not None:
    from ras_db import RasDatabase, RasDatabaseCommand  # noqa: E402
    from ras_filter import (DatabaseFilter, DatabaseOrder,
                            RasDatabaseQuery)  # noqa: E402


class RasDatabaseTests:
    """Tests shared by the SQLite, MySQL and PostgreSQL groups."""

    backend = None
    DATABASE_SOURCES = (
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
        sources = {root / filename for filename in cls.DATABASE_SOURCES}

        sources.update(root.glob("events*/**/*-handler.c"))
        descriptors = {}
        for filename in sorted(sources):
            source = filename.read_text(encoding="utf-8")
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
                "use_ssl": os.environ.get("RAS_MYSQL_USE_SSL", "false"),
                "connect_timeout": os.environ.get(
                    "RAS_MYSQL_CONNECT_TIMEOUT", "10"
                ),
            }}
        if self.backend == "postgresql":
            return {"postgresql_conn_parms": {
                "host": os.environ.get("RAS_PG_HOST", ""),
                "port": os.environ.get("RAS_PG_PORT", "5432"),
                "user": os.environ.get("RAS_PG_USER", "rasdaemon"),
                "password": os.environ.get("RAS_PG_PASSWORD", ""),
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
        query = self.database._query({})
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
                                value = query._database_timestamp(
                                    f"2026-03-{row + 1:02d} 10:00:00"
                                )
                        elif (table.name == "mc_event"
                              and column.name == "err_type"):
                            value = ("Corrected" if row < 5
                                     else "Uncorrected")
                        elif (table.name == "aer_event"
                              and column.name == "err_type"):
                            value = (
                                "Corrected", "Uncorrected (Non-Fatal)",
                                "Uncorrected (Fatal)", "Corrected", "Corrected",
                            )[row % 5]
                        elif (table.name == "non_standard_event"
                              and column.name == "severity"):
                            value = (
                                "Corrected", "Recoverable", "Fatal",
                                "Informational", "Corrected",
                            )[row % 5]
                        elif (table.name == "extlog_event"
                              and column.name == "severity"):
                            value = (2, 1, 0, 3, 2)[row % 5]
                        elif (table.name == "reri_event"
                              and column.name == "severity"):
                            value = (1, 3, 2, 0, 1)[row % 5]
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

    def test_table_filters(self):
        table_names = sorted(self.descriptors)
        exact = table_names[0]

        self.assertEqual(list(self.database.select_tables([exact])), [exact])
        event_tables = self.database.select_tables(["*_event"])
        self.assertTrue(event_tables)
        self.assertTrue(all(name.endswith("_event") for name in event_tables))
        filtered = self.database.select_tables(["*"], ["cxl_*"])
        self.assertTrue(filtered)
        self.assertTrue(all(not name.startswith("cxl_") for name in filtered))

        with self.assertRaisesRegex(ValueError, "unknown event table"):
            self.database.select_tables(["missing_event_table"])
        self.assertEqual(self.database.select_tables(["missing_*"]), {})

    def test_selected_indexes(self):
        selected_name = sorted(self.descriptors)[0]
        selected = self.database.select_tables([selected_name])
        created = self.database.create_missing_indexes(selected)
        columns_per_table = 1 if self.backend == "sqlite3" else 2

        self.assertEqual(len(created), columns_per_table)
        inspector = sqlalchemy.inspect(self.engine)
        self.assertTrue(inspector.get_indexes(
            selected_name, schema=self.database.schema
        ))
        other_name = next(
            name for name in sorted(self.descriptors) if name != selected_name
        )
        self.assertEqual(inspector.get_indexes(
            other_name, schema=self.database.schema
        ), [])

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

    def test_table_summary(self):
        selected_name = sorted(self.descriptors)[0]
        selected = self.database.select_tables([selected_name])
        groups = self.database.summary(
            since="2026-03-03 10:00:00", until="2026-03-05 10:00:00",
            tables=selected,
        )

        self.assertEqual(list(groups), ["local-host"])
        self.assertEqual(groups["local-host"], {selected_name: 3})
        formatted = self.database.format_summary(groups)
        self.assertIn(f"{selected_name}: 3 event(s)", formatted)

    def test_dynamic_filter_uses_only_tables_with_the_field(self):
        target = "mc_event-label-0"
        groups = self.database.records(
            filters=RasDatabaseQuery.parse_filters([f"label={target}"]),
            select_fields=("label",),
        )

        self.assertEqual(list(groups), ["local-host"])
        self.assertEqual(len(groups["local-host"]), 1)
        event = groups["local-host"][0]
        self.assertEqual(event.table, "mc_event")
        self.assertEqual(event.values["label"], target)
        self.assertEqual(
            self.database.format_records(groups, ("label",)),
            f"Hostname: local-host\n  {event.timestamp} mc_event: label={target}\n",
        )

    def test_count_and_group_by_filters(self):
        selected = self.database.select_tables(["mc_event"])
        counts = self.database.counts(
            tables=selected, severity="corrected",
        )
        self.assertEqual(len(counts), 1)
        self.assertEqual(counts[0].count, 5)
        self.assertEqual(self.database.format_counts(counts), "Count: 5\n")

        counts = self.database.counts(
            tables=selected, severity="corrected", group_by=("label",),
            order_by=(DatabaseOrder("count", descending=True),
                      DatabaseOrder("label")),
        )
        self.assertEqual(len(counts), 5)
        self.assertTrue(all(count.count == 1 for count in counts))
        self.assertEqual(
            [count.values["label"] for count in counts],
            [f"mc_event-label-{row}" for row in range(5)],
        )

    def test_count_by_table_only_reports_non_empty_tables(self):
        counts = self.database.counts(
            filters=RasDatabaseQuery.parse_filters(["label=mc_event-label-0"]),
            group_by=("table",),
        )

        self.assertEqual(len(counts), 1)
        self.assertEqual(counts[0].values, {"table": "mc_event"})
        self.assertEqual(counts[0].count, 1)

    def test_severity_shortcuts_cover_aer_and_ghes_tables(self):
        expectations = {
            "aer_event": {"corrected": 6, "uncorrected": 4, "fatal": 2},
            "non_standard_event": {"corrected": 4, "recoverable": 2,
                                   "fatal": 2, "info": 2},
            "extlog_event": {"corrected": 4, "recoverable": 2,
                             "fatal": 2, "info": 2},
            "reri_event": {"corrected": 4, "recoverable": 2,
                           "fatal": 2, "info": 2},
        }
        for table_name, severities in expectations.items():
            selected = self.database.select_tables([table_name])
            for severity, expected in severities.items():
                count = self.database.counts(
                    tables=selected, severity=severity,
                )
                self.assertEqual(count[0].count, expected, (table_name, severity))

    def test_detailed_ordering_uses_runtime_field(self):
        selected = self.database.select_tables(["mc_event"])
        groups = self.database.records(
            tables=selected, severity="corrected",
            select_fields=("label", "err_type"),
            order_by=(DatabaseOrder("label", descending=True),),
        )

        labels = [event.values["label"] for event in groups["local-host"]]
        self.assertEqual(labels, sorted(labels, reverse=True))

    def test_database_command_options_depend_on_backend(self):
        parser = argparse.ArgumentParser()
        subparsers = parser.add_subparsers()
        database_command = RasDatabaseCommand("ras-mc-ctl", subparsers)
        command = ["database", "--since", "2026-03-01",
                   "--until", "2026-03-02", "--summary",
                   "--table", "cxl_*", "--except", "*_dram_*"]
        if self.backend != "sqlite3":
            command.extend(["--hostname", "remote-a"])
        args = parser.parse_args(command)
        self.assertEqual(args.since, "2026-03-01")
        self.assertEqual(args.until, datetime.date(2026, 3, 2))
        self.assertTrue(args.summary)
        self.assertEqual(args.table, ["cxl_*"])
        self.assertEqual(args.exclude_table, ["*_dram_*"])
        expected_hostname = None if self.backend == "sqlite3" else "remote-a"
        self.assertEqual(args.hostname, expected_hostname)

        errors = parser.parse_args([
            "database", "--errors", "--table", "mc_event"
        ])
        self.assertTrue(errors.errors)
        self.assertEqual(errors.table, ["mc_event"])
        self.assertTrue(parser.parse_args([
            "database", "--list-tables"
        ]).list_tables)
        self.assertTrue(parser.parse_args([
            "database", "--create-index"
        ]).create_index)
        self.assertTrue(parser.parse_args([
            "database", "-E"
        ]).errors_per_table)
        self.assertTrue(parser.parse_args([
            "database", "--summary", "--json"
        ]).json)

        count = parser.parse_args([
            "database", "--count", "--corrected", "--where", "label=DIMM0",
            "--group-by", "label", "--order-by", "count:desc", "--verbose",
        ])
        self.assertTrue(count.count)
        self.assertEqual(count.severity, "corrected")
        self.assertEqual(count.where, ["label=DIMM0"])
        self.assertEqual(count.group_by, ["label"])
        self.assertEqual(count.order_by, ["count:desc"])
        self.assertEqual(count.verbose, 1)
        self.assertIn("Count corrected EDAC events",
                      database_command.parser.format_help())

    def test_json_formatters_produce_versioned_documents(self):
        tables = self.database.discover_tables()
        groups = self.database.records(
            tables={"mc_event": tables["mc_event"]}
        )
        records = json.loads(self.database.format_records_json(groups))
        self.assertEqual(records["format_version"], 1)
        self.assertEqual(records["mode"], "errors")
        self.assertTrue(records["records"])
        self.assertEqual(records["records"][0]["table"], "mc_event")
        self.assertIn("hostname", records["records"][0])
        self.assertIn("timestamp", records["records"][0])
        self.assertIn("fields", records["records"][0])

        counts = self.database.counts(
            tables={"mc_event": tables["mc_event"]},
            group_by=("table",),
        )
        aggregate = json.loads(
            self.database.format_counts_json("errors-per-table", counts)
        )
        self.assertEqual(aggregate["format_version"], 1)
        self.assertEqual(aggregate["mode"], "errors-per-table")
        self.assertEqual(aggregate["groups"][0]["values"]["table"],
                         "mc_event")

    def test_json_formatter_encodes_binary_values(self):
        document = json.loads(RasDatabase.format_json(
            "test", {"blob": b"ras"}
        ))
        self.assertEqual(document["blob"], {
            "encoding": "base64", "data": "cmFz",
        })

    def test_filter_parser_rejects_raw_sql(self):
        with self.assertRaisesRegex(ValueError, "invalid filter"):
            RasDatabaseQuery.parse_filters(["label; DROP TABLE mc_event"])

    def test_verbose_query_planning_explains_excluded_tables(self):
        with self.assertLogs("ras_db", "DEBUG") as logs:
            self.database.counts(
                filters=RasDatabaseQuery.parse_filters(["label=mc_event-label-0"])
            )

        self.assertTrue(any("Using event table mc_event" in item
                            for item in logs.output))
        self.assertTrue(any("Skipping event table aer_event" in item
                            for item in logs.output))

    def test_database_command_extends_until_to_next_midnight(self):
        parser = argparse.ArgumentParser()
        subparsers = parser.add_subparsers()
        command = RasDatabaseCommand("ras-mc-ctl", subparsers)
        database = mock.Mock()
        database.select_tables.return_value = {}
        database.records.return_value = {}
        database.format_records.return_value = ""

        with mock.patch.object(RasDatabase, "from_config", return_value=database):
            command.run(None, parser.parse_args([
                "database", "--until", "2026-03-05"
            ]))

        database.records.assert_called_once_with(
            since=None, until="2026-03-06 00:00:00",
            hostname=None, tables={}, filters=(), severity=None,
            select_fields=(), order_by=(),
        )

    def test_database_command_creates_indexes_only_on_request(self):
        parser = argparse.ArgumentParser()
        subparsers = parser.add_subparsers()
        command = RasDatabaseCommand("ras-mc-ctl", subparsers)
        database = mock.Mock()
        database.select_tables.return_value = {}
        database.summary.return_value = {}
        database.format_summary.return_value = ""
        database.create_missing_indexes.return_value = ["idx_mc_event_timestamp"]

        with mock.patch.object(RasDatabase, "from_config", return_value=database):
            command.run(None, parser.parse_args(["database", "--summary"]))
            database.create_missing_indexes.assert_not_called()
            database.close.assert_called_once()

            database.reset_mock()
            command.run(None, parser.parse_args([
                "database", "--create-index"
            ]))
            database.create_missing_indexes.assert_called_once_with({})
            database.close.assert_called_once()

    def test_database_count_command_passes_query_options(self):
        parser = argparse.ArgumentParser()
        subparsers = parser.add_subparsers()
        command = RasDatabaseCommand("ras-mc-ctl", subparsers)
        database = mock.Mock()
        database.select_tables.return_value = {}
        database.counts.return_value = []
        database.format_counts.return_value = "Count: 0\n"

        with mock.patch.object(RasDatabase, "from_config", return_value=database):
            with contextlib.redirect_stdout(io.StringIO()):
                command.run(None, parser.parse_args([
                    "database", "--count", "--corrected", "--where", "label=A1",
                    "--group-by", "label", "--order-by", "count:desc",
                ]))

        database.counts.assert_called_once_with(
            since=None, until=None, hostname=None, tables={},
            filters=(DatabaseFilter("label", "=", "A1"),),
            severity="corrected", group_by=("label",),
            order_by=(DatabaseOrder("count", descending=True),),
        )
        database.format_counts.assert_called_once_with([], ("label",))

    def test_database_errors_per_table_uses_table_count_grouping(self):
        parser = argparse.ArgumentParser()
        subparsers = parser.add_subparsers()
        command = RasDatabaseCommand("ras-mc-ctl", subparsers)
        database = mock.Mock()
        database.select_tables.return_value = {}
        database.counts.return_value = []
        database.format_counts.return_value = ""

        with mock.patch.object(RasDatabase, "from_config", return_value=database):
            with contextlib.redirect_stdout(io.StringIO()):
                command.run(None, parser.parse_args([
                    "database", "-E", "--corrected", "--where", "label=A1",
                    "--order-by", "count:desc",
                ]))

        database.counts.assert_called_once_with(
            since=None, until=None, hostname=None, tables={},
            filters=(DatabaseFilter("label", "=", "A1"),),
            severity="corrected", group_by=("table",),
            order_by=(DatabaseOrder("count", descending=True),),
        )
        database.format_counts.assert_called_once_with([], ("table",))


@unittest.skipIf(sqlalchemy is None, "SQLAlchemy is not installed")
class SqliteRasDatabaseTest(RasDatabaseTests, unittest.TestCase):
    backend = "sqlite3"

    def test_malformed_utf8_text_is_displayable(self):
        table = sqlalchemy.Table(
            "malformed_utf8_event", self.metadata,
            sqlalchemy.Column("id", sqlalchemy.Integer, primary_key=True),
            sqlalchemy.Column("timestamp", sqlalchemy.Text),
            sqlalchemy.Column("err_info", sqlalchemy.Text),
        )
        table.create(self.engine)
        with self.engine.begin() as connection:
            connection.exec_driver_sql(
                f'INSERT INTO "{table.name}" '
                "(id, timestamp, err_info) VALUES "
                "(1, '2026-03-01 10:00:00', CAST(X'80' AS TEXT))"
            )

        groups = self.database.records()
        event = next(
            item for item in groups["local-host"]
            if item.table == table.name and item.values["id"] == 1
        )
        self.assertEqual(event.values["err_info"], r"\x80")
        self.assertIn(r"err_info=\x80", self.database.format_records(groups))

    def test_database_url_uses_complete_path(self):
        path = os.path.join(self.temporary_directory.name, "complete-path.db")
        url = RasDatabase._database_url(
            "sqlite3", {"sqlite3_database": path}
        )

        self.assertEqual(url.database, path)


@unittest.skipIf(sqlalchemy is None, "SQLAlchemy is not installed")
class PostgresqlUrlContractTest(unittest.TestCase):
    def test_defaults_to_unix_socket(self):
        url = RasDatabase._database_url("postgresql", {
            "postgresql_conn_parms": {},
        })

        self.assertIsNone(url.host)
        self.assertEqual(url.database, "rasdaemon")
        self.assertNotIn("sslmode", url.query)

    def test_ssl_mode_matches_daemon_contract(self):
        url = RasDatabase._database_url("postgresql", {
            "postgresql_conn_parms": {
                "ssl_mode": "verify-full",
                "use_ssl": "false",
            },
        })
        self.assertEqual(url.query["sslmode"], "verify-full")

        url = RasDatabase._database_url("postgresql", {
            "postgresql_conn_parms": {
                "ssl_mode": "false",
                "use_ssl": "true",
            },
        })
        self.assertEqual(url.query["sslmode"], "require")

    def test_connections_use_utc_for_date_boundaries(self):
        self.assertEqual(
            RasDatabase._database_connect_args("postgresql", {}),
            {"options": "-c timezone=UTC"},
        )


@unittest.skipIf(sqlalchemy is None, "SQLAlchemy is not installed")
class TimestampConversionTest(unittest.TestCase):
    def setUp(self):
        self.old_timezone = os.environ.get("TZ")
        # Take the test to French Polynesia (UTC-09:30), as even
        # server machines deserve island vacations now and then... ;-)
        os.environ["TZ"] = "Pacific/Marquesas"
        time.tzset()

    def tearDown(self):
        if self.old_timezone is None:
            os.environ.pop("TZ", None)
        else:
            os.environ["TZ"] = self.old_timezone
        time.tzset()

    @staticmethod
    def query(backend):
        return RasDatabaseQuery(backend, "local-host", {}, mock.Mock())

    def test_local_query_bound_is_converted_to_backend_utc(self):
        mysql = self.query("mysql")._database_timestamp(
            "2026-03-05 10:00:00"
        )
        postgresql = self.query("postgresql")._database_timestamp(
            "2026-03-05 10:00:00"
        )

        self.assertEqual(mysql, datetime.datetime(2026, 3, 5, 19, 30))
        self.assertEqual(
            postgresql,
            datetime.datetime(
                2026, 3, 5, 19, 30, tzinfo=datetime.timezone.utc
            ),
        )

    def test_backend_utc_timestamp_is_displayed_in_local_time(self):
        expected = datetime.datetime(
            2026, 3, 5, 10,
            tzinfo=datetime.timezone(datetime.timedelta(hours=-9, minutes=-30)),
        )

        mysql = self.query("mysql")._local_timestamp(
            datetime.datetime(2026, 3, 5, 19, 30)
        )
        postgresql = self.query("postgresql")._local_timestamp(
            datetime.datetime(
                2026, 3, 5, 19, 30, tzinfo=datetime.timezone.utc
            )
        )

        self.assertEqual(mysql, expected)
        self.assertEqual(postgresql, expected)

    def test_sqlite_storage_is_unchanged_but_display_is_normalized(self):
        timestamp = "2026-03-05 10:00:00 -0930"
        query = self.query("sqlite3")

        self.assertEqual(query._database_timestamp(timestamp), timestamp)
        self.assertEqual(
            query._local_timestamp(timestamp),
            datetime.datetime(
                2026, 3, 5, 10,
                tzinfo=datetime.timezone(
                    datetime.timedelta(hours=-9, minutes=-30)
                ),
            ),
        )


@unittest.skipIf(sqlalchemy is None, "SQLAlchemy is not installed")
class MysqlUrlContractTest(unittest.TestCase):
    def test_defaults_match_the_daemon(self):
        url = RasDatabase._database_url("mysql", {
            "mysql_conn_parms": {},
        })

        self.assertIsNone(url.host)
        self.assertEqual(url.database, "rasdaemon")
        self.assertEqual(url.query["connect_timeout"], "10")

    def test_timeout_and_tls_configuration(self):
        settings = {"mysql_conn_parms": {
            "connect_timeout": "7",
            "use_ssl": "true",
        }}
        url = RasDatabase._database_url("mysql", settings)

        self.assertEqual(url.query["connect_timeout"], "7")
        self.assertEqual(
            RasDatabase._database_connect_args("mysql", settings),
            {"ssl": {}},
        )

    def test_tls_verification_rejects_an_unencrypted_connection(self):
        cursor = mock.Mock()
        connection = mock.Mock()
        connection.cursor.return_value = cursor

        cursor.fetchone.return_value = ("Ssl_cipher", "TLS_AES_256_GCM_SHA384")
        RasDatabase._require_mysql_tls(connection)
        cursor.close.assert_called_once()

        cursor.reset_mock()
        cursor.fetchone.return_value = ("Ssl_cipher", "")
        with self.assertRaisesRegex(RuntimeError, "TLS was required"):
            RasDatabase._require_mysql_tls(connection)
        cursor.close.assert_called_once()


@unittest.skipIf(sqlalchemy is None, "SQLAlchemy is not installed")
class MysqlRasDatabaseTest(RasDatabaseTests, unittest.TestCase):
    backend = "mysql"


@unittest.skipIf(sqlalchemy is None, "SQLAlchemy is not installed")
class PostgresqlRasDatabaseTest(RasDatabaseTests, unittest.TestCase):
    backend = "postgresql"


if __name__ == "__main__":
    unittest.main()
