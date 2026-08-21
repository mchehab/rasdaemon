# SPDX-License-Identifier: GPL-2.0

import contextlib
import io
import os
import runpy
import sqlite3
import sys
import tempfile
import unittest
from unittest import mock

from sqlalchemy.dialects import mysql, postgresql, sqlite
from sqlalchemy.schema import CreateTable


ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPT = os.path.join(ROOT_DIR, "contrib", "fill-test-databases")
SCRIPT_GLOBALS = runpy.run_path(SCRIPT, run_name="fill_test_databases")
TestDatabaseFiller = SCRIPT_GLOBALS["TestDatabaseFiller"]
parse_args = SCRIPT_GLOBALS["parse_args"]


class FillTestDatabasesTest(unittest.TestCase):
    def setUp(self):
        self.filler = TestDatabaseFiller(["sqlite3"])
        self.definitions = self.filler.descriptors()

    def test_default_cli_selection_means_all_backends(self):
        with mock.patch.object(sys, "argv", [SCRIPT]):
            args = parse_args()
        filler = TestDatabaseFiller(args.database_backend)
        self.assertEqual(filler.backends, filler.BACKENDS)

    def test_selected_descriptors(self):
        self.assertEqual(set(self.definitions), set(self.filler.TABLES))

    def test_definitions_compile_for_all_dialects(self):
        cases = (
            ("sqlite3", sqlite.dialect(), None),
            ("mysql", mysql.dialect(), None),
            ("postgresql", postgresql.dialect(), "rasdaemon"),
        )
        for backend, dialect, schema in cases:
            with self.subTest(backend=backend):
                metadata = self.filler.define_tables(
                    backend, schema, self.definitions
                )
                ddl = str(CreateTable(
                    next(iter(metadata.tables.values()))
                ).compile(dialect=dialect))
                self.assertIn("PRIMARY KEY", ddl)
                self.assertEqual(
                    "hostname" in ddl, backend != "sqlite3"
                )

    def test_sqlite_fill_decline_and_append(self):
        with tempfile.TemporaryDirectory() as directory:
            database = os.path.join(directory, "events.db")
            with mock.patch.dict(
                    os.environ, {"RAS_SQLITE3_DATABASE": database}):
                with (contextlib.redirect_stdout(io.StringIO()),
                      contextlib.redirect_stderr(io.StringIO())):
                    self.filler.populate("sqlite3", self.definitions)
                    with mock.patch("builtins.input", return_value="no"):
                        self.filler.populate("sqlite3", self.definitions)
                    with mock.patch("builtins.input", return_value="yes"):
                        self.filler.populate("sqlite3", self.definitions)

            with sqlite3.connect(database) as connection:
                names = [row[0] for row in connection.execute(
                    "SELECT name FROM sqlite_master WHERE type='table' "
                    "AND name != 'sqlite_sequence'"
                )]
                counts = [connection.execute(
                    f'SELECT COUNT(*) FROM "{name}"'
                ).fetchone()[0] for name in names]

            self.assertEqual(len(names), len(self.filler.TABLES))
            self.assertEqual(counts, [100] * len(self.filler.TABLES))

    def test_eof_declines_append(self):
        counts = {name: 50 for name in self.filler.TABLES}
        with (mock.patch("builtins.input", side_effect=EOFError),
              contextlib.redirect_stderr(io.StringIO())):
            self.assertFalse(self.filler.confirm_append("sqlite3", counts))


if __name__ == "__main__":
    unittest.main()
