# SPDX-License-Identifier: GPL-2.0

import argparse
import contextlib
import io
import os
import pathlib
import sys
import tempfile
import unittest
from unittest import mock


UTIL_DIR = pathlib.Path(__file__).resolve().parents[1] / "util"
sys.path.insert(0, str(UTIL_DIR))

from ras_dimm import RasMemoryDimm  # noqa: E402


class RasMemoryDimmTest(unittest.TestCase):
    def setUp(self):
        parser = argparse.ArgumentParser()
        self.dimm = RasMemoryDimm("ras-mc-ctl", parser.add_subparsers())

    def test_parse_board_dmidecode_prefers_baseboard(self):
        output = """System Information
\tManufacturer: System vendor
\tProduct Name: System model

Base Board Information
\tManufacturer: Board vendor
\tProduct Name: Board model
"""
        self.assertEqual(
            self.dimm._parse_board_dmidecode(output),
            ("Board vendor", "Board model"),
        )

    @mock.patch("ras_dimm.which", return_value="/usr/sbin/dmidecode")
    @mock.patch("ras_dimm.os.geteuid", return_value=1000)
    @mock.patch("ras_dimm.subprocess.run")
    def test_dmidecode_permission_error_does_not_invoke_sudo(
            self, run, _geteuid, _which):
        errors = io.StringIO()
        with contextlib.redirect_stderr(errors):
            self.assertIsNone(self.dimm._run_dmidecode())
        run.assert_not_called()
        self.assertIn("please run this command using sudo", errors.getvalue())

    def test_label_database_and_sysfs_match_perl_layout(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            labels = root / "labels.db"
            labels.write_text(
                "Vendor: Acme\n"
                "  Model: Board 1\n"
                "    A1: 0.0.0; A2: 0.0.1;\n",
                encoding="utf-8",
            )
            dimm = root / "mc0" / "dimm0"
            dimm.mkdir(parents=True)
            (dimm / "dimm_location").write_text(
                "channel 0 slot 0\n", encoding="utf-8"
            )
            (dimm / "dimm_label").write_text("old\n", encoding="utf-8")

            self.dimm.label_db = os.fspath(labels)
            self.dimm.label_dir = None
            self.dimm.SYSFS_DIR = os.fspath(root)
            self.dimm.mainboard_vendor = "ACME"
            self.dimm.mainboard_model = "board 1"

            entry = self.dimm._read_dimm_labels()
            self.assertEqual(entry["layers"], 2)
            self.assertEqual(entry["labels"][(0, 0, 0, 0)], "A1")
            self.assertEqual(entry["labels"][(0, 0, 1, 0)], "A2")

            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertTrue(self.dimm.print_dimm_labels())
            self.assertIn("mc0 channel 0 slot 0", output.getvalue())
            self.assertIn("A1", output.getvalue())
            self.assertIn("old", output.getvalue())
            self.assertIn("0:0:1 missing", output.getvalue())

            self.assertTrue(self.dimm.register_dimm_labels())
            self.assertEqual((dimm / "dimm_label").read_text(encoding="utf-8"),
                             "A1")

    def test_guess_labels_parses_locator_and_bank_locator(self):
        output = """Memory Device
\tLocator: DIMM_A1
\tBank Locator: BANK 0

Memory Device
\tLocator: DIMM_B1
\tBank Locator: BANK 1
"""
        with mock.patch.object(self.dimm, "_run_dmidecode", return_value=output):
            printed = io.StringIO()
            with contextlib.redirect_stdout(printed):
                self.assertTrue(self.dimm.guess_dimm_label())
        self.assertEqual(
            printed.getvalue().splitlines(),
            ["memory stick 'DIMM_A1' is located at 'BANK 0'",
             "memory stick 'DIMM_B1' is located at 'BANK 1'"],
        )


if __name__ == "__main__":
    unittest.main()
