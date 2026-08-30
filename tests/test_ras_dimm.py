# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

import argparse
import contextlib
import io
import os
import pathlib
import sys
import tempfile
import unittest

from textwrap import dedent
from unittest import mock


UTIL_DIR = pathlib.Path(__file__).resolve().parents[1] / "util"
sys.path.insert(0, str(UTIL_DIR))

from ras_dimm import RasMemoryDimm  # noqa: E402


class RasMemoryDimmTest(unittest.TestCase):
    def setUp(self):
        parser = argparse.ArgumentParser()
        self.parser = parser
        self.dimm = RasMemoryDimm("ras-mc-ctl", parser.add_subparsers())

    def test_parse_board_dmidecode_prefers_baseboard(self):
        output = dedent("""\
            System Information
            \tManufacturer: System vendor
            \tProduct Name: System model

            Base Board Information
            \tManufacturer: Board vendor
            \tProduct Name: Board model
            """)
        self.assertEqual(
            self.dimm._parse_board_dmidecode(output),
            ("Board vendor", "Board model"),
        )

    def test_uses_configured_sysconf_directory(self):
        parser = argparse.ArgumentParser()
        dimm = RasMemoryDimm(
            "ras-mc-ctl", parser.add_subparsers(), "/opt/ras/etc"
        )

        self.assertEqual(dimm.label_db, "/opt/ras/etc/ras/dimm_labels.db")
        self.assertEqual(dimm.label_dir, "/opt/ras/etc/ras/dimm_labels.d")
        self.assertEqual(dimm.mainboard_config, "/opt/ras/etc/ras/mainboard")

    def test_mainboard_config_precedes_sysfs_and_dmidecode(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            config_dir = root / "etc" / "ras"
            config_dir.mkdir(parents=True)
            (config_dir / "mainboard").write_text(
                "# Local override\n"
                "vendor = Configured Vendor\n"
                "model = Configured Model # trailing comment\n",
                encoding="utf-8",
            )
            dmi_dir = root / "dmi"
            dmi_dir.mkdir()
            (dmi_dir / "board_vendor").write_text(
                "Sysfs Vendor\n", encoding="utf-8"
            )
            (dmi_dir / "board_name").write_text(
                "Sysfs Model\n", encoding="utf-8"
            )
            self.dimm.mainboard_config = os.fspath(config_dir / "mainboard")
            self.dimm.DMI_DIR = os.fspath(dmi_dir)

            with mock.patch.object(self.dimm, "_run_dmidecode") as dmidecode:
                self.assertTrue(self.dimm.get_mainboard_info())

        dmidecode.assert_not_called()
        self.assertEqual(self.dimm.mainboard_vendor, "Configured Vendor")
        self.assertEqual(self.dimm.mainboard_model, "Configured Model")

    def test_mainboard_config_may_call_legacy_helper_script(self):
        with tempfile.TemporaryDirectory() as directory:
            config = pathlib.Path(directory) / "mainboard"
            config.write_text("script = /usr/libexec/ras-board\n", encoding="utf-8")
            self.dimm.mainboard_config = os.fspath(config)
            result = argparse.Namespace(
                returncode=0,
                stdout="vendor=Script Vendor\nmodel=Script Model\n",
                stderr="",
            )

            with mock.patch("ras_dimm.subprocess.run", return_value=result) as run:
                values = self.dimm._mainboard_config_values()

        self.assertEqual(values, ("Script Vendor", "Script Model"))
        run.assert_called_once_with(
            "/usr/libexec/ras-board", shell=True, capture_output=True,
            text=True, check=False,
        )

    def test_parse_board_dmidecode_asus_b650m(self):
        output = dedent("""\
            Handle 0x0001, DMI type 1, 27 bytes
            System Information
                    Manufacturer: ASUS
                    Product Name: System Product Name
                    Version: System Version
                    Serial Number: System Serial Number
                    UUID: 1ea2757b-548a-d464-acaf-08bfb8753f32
                    Wake-up Type: Power Switch
                    SKU Number: SKU
                    Family: To be filled by O.E.M.

            Handle 0x0002, DMI type 2, 15 bytes
            Base Board Information
                    Manufacturer: ASUSTeK COMPUTER INC.
                    Product Name: TUF GAMING B650M-E WIFI
                    Version: Rev 1.xx
                    Serial Number: 230419116700014
                    Asset Tag: Default string
                    Features:
                            Board is a hosting board
                            Board is replaceable
                    Location In Chassis: Default string
                    Chassis Handle: 0x0003
                    Type: Motherboard
                    Contained Object Handles: 0
            """)
        self.assertEqual(
            self.dimm._parse_board_dmidecode(output),
            ("ASUSTeK COMPUTER INC.", "TUF GAMING B650M-E WIFI"),
        )

    def test_parse_board_dmidecode_huawei_2288x_v5(self):
        output = dedent("""\
            # dmidecode 3.5
            Getting SMBIOS data from sysfs.
            SMBIOS 3.0.0 present.

            Handle 0x0001, DMI type 1, 27 bytes
            System Information
            \tManufacturer: Huawei
            \tProduct Name: 2288X V5
            \tVersion: Purley
            \tSerial Number: 2102313BYX10M3000250

            Handle 0x0002, DMI type 2, 15 bytes
            Base Board Information
            \tManufacturer: Huawei
            \tProduct Name: BC11SPSFB0
            \tVersion: V100R005
            \tSerial Number: 028HTR10M3000178
            \tAsset Tag: Huawei
            \tFeatures:
            \t\tBoard is a hosting board
            \t\tBoard is replaceable
            \tLocation In Chassis: Type2 - Board Chassis Location
            \tChassis Handle: 0x0003
            \tType: Motherboard
            \tContained Object Handles: 0
            """)
        self.assertEqual(
            self.dimm._parse_board_dmidecode(output),
            ("Huawei", "BC11SPSFB0"),
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
        output = dedent("""\
            Memory Device
            \tLocator: DIMM_A1
            \tBank Locator: BANK 0

            Memory Device
            \tLocator: DIMM_B1
            \tBank Locator: BANK 1
            """)
        with mock.patch.object(self.dimm, "_run_dmidecode", return_value=output):
            printed = io.StringIO()
            with contextlib.redirect_stdout(printed):
                self.assertTrue(self.dimm.guess_dimm_label())
        self.assertEqual(
            printed.getvalue().splitlines(),
            ["memory stick 'DIMM_A1' is located at 'BANK 0'",
             "memory stick 'DIMM_B1' is located at 'BANK 1'"],
        )

    @staticmethod
    def _add_counter_dimm(root, controller, dimm, location, label, ce, ue):
        directory = root / f"mc{controller}" / f"dimm{dimm}"
        directory.mkdir(parents=True)
        (directory / "dimm_location").write_text(
            f"{location}\n", encoding="utf-8"
        )
        (directory / "dimm_label").write_text(f"{label}\n", encoding="utf-8")
        (directory / "dimm_ce_count").write_text(f"{ce}\n", encoding="utf-8")
        (directory / "dimm_ue_count").write_text(f"{ue}\n", encoding="utf-8")

    def test_error_count_consolidates_same_label(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            self._add_counter_dimm(root, 0, 0, "channel 0 slot 0", "A1", 2, 3)
            self._add_counter_dimm(root, 0, 1, "channel 1 slot 0", "A1", 5, 7)
            self._add_counter_dimm(root, 1, 0, "channel 0 slot 0", "B1", 11, 13)
            self.dimm.SYSFS_DIR = os.fspath(root)

            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertTrue(self.dimm.display_error_count())

        self.assertEqual(
            output.getvalue(),
            "Label\tCE\tUE\nA1\t7\t10\nB1\t11\t13\n",
        )

    def test_error_count_per_rank_keeps_locations_separate(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            self._add_counter_dimm(root, 0, 0, "channel 0 slot 0", "A1", 2, 3)
            self._add_counter_dimm(root, 0, 1, "channel 1 slot 0", "A1", 5, 7)
            self.dimm.SYSFS_DIR = os.fspath(root)

            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertTrue(self.dimm.display_error_count(per_rank=True))

        self.assertEqual(
            output.getvalue(),
            "Label".ljust(21) + "\tCE\tUE\n"
            "A1 (channel 0 slot 0)\t2\t3\n"
            "A1 (channel 1 slot 0)\t5\t7\n",
        )

    def test_error_count_command_options(self):
        args = self.parser.parse_args([
            "dimm", "--error-count", "--per-rank"
        ])
        self.assertTrue(args.error_count)
        self.assertTrue(args.per_rank)

        with mock.patch.object(
                self.dimm, "display_error_count", return_value=True) as display:
            self.assertEqual(self.dimm.run(None, args), 0)
        display.assert_called_once_with(True)

    def test_per_rank_requires_error_count(self):
        args = self.parser.parse_args(["dimm", "--per-rank"])
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                self.dimm.run(None, args)

    def test_delay_requires_register_labels(self):
        args = self.parser.parse_args(["dimm", "--status", "--delay", "1"])
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                self.dimm.run(None, args)

    def test_negative_delay_is_rejected(self):
        args = self.parser.parse_args([
            "dimm", "--register-labels", "--delay", "-1"
        ])
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                self.dimm.run(None, args)

    def test_failed_status_sets_command_exit_status(self):
        args = self.parser.parse_args(["dimm", "--status"])
        with mock.patch.object(self.dimm, "print_status", return_value=False):
            self.assertEqual(self.dimm.run(None, args), 1)

    def test_failed_error_count_sets_command_exit_status(self):
        args = self.parser.parse_args(["dimm", "--error-count"])
        with mock.patch.object(
                self.dimm, "display_error_count", return_value=False):
            self.assertEqual(self.dimm.run(None, args), 1)

    def test_quiet_suppresses_status_and_errors(self):
        args = self.parser.parse_args(["dimm", "--status", "--quiet"])
        output = io.StringIO()
        with mock.patch("ras_dimm.os.scandir", side_effect=OSError):
            with contextlib.redirect_stdout(output):
                self.assertEqual(self.dimm.run(None, args), 1)
        self.assertEqual(output.getvalue(), "")

        with mock.patch.object(self.dimm, "_read_dimm_labels", return_value=None):
            errors = io.StringIO()
            with contextlib.redirect_stderr(errors):
                self.assertFalse(self.dimm.print_dimm_labels())
        self.assertEqual(errors.getvalue(), "")

    def test_combined_actions_keep_legacy_order_and_collect_failures(self):
        args = self.parser.parse_args([
            "dimm", "--layout", "--guess-labels", "--error-count", "--status"
        ])
        calls = []

        def action(name, result=True):
            def run(*_args):
                calls.append(name)
                return result
            return run

        with mock.patch.object(
                self.dimm, "display_memory_layout", side_effect=action("layout")):
            with mock.patch.object(
                    self.dimm, "guess_dimm_label",
                    side_effect=action("guess", False)):
                with mock.patch.object(
                        self.dimm, "display_error_count",
                        side_effect=action("error-count")):
                    with mock.patch.object(
                            self.dimm, "print_status",
                            side_effect=action("status")):
                        self.assertEqual(self.dimm.run(None, args), 1)

        self.assertEqual(calls, ["layout", "guess", "error-count", "status"])


if __name__ == "__main__":
    unittest.main()
