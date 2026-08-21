# SPDX-License-Identifier: GPL-2.0

import io
import os
import sys
import tempfile
import unittest


PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(PROJECT_DIR, "util"))

from mem_layout import MemoryLayout


class MemoryLayoutTest(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.sysfs = self.temporary_directory.name

        os.makedirs(os.path.join(self.sysfs, "mc0", "csrow0"))
        self._write("mc0/csrow0/size_mb", "3072\n")
        self._write("mc0/max_location", "channel 1 slot 0\n")

        for dimm, channel, size in ((0, 0, 1024), (1, 1, 2048)):
            self._add_dimm(dimm, channel, size)

    def _add_dimm(self, dimm, channel, size, slot=0):
        directory = os.path.join(self.sysfs, "mc0", f"dimm{dimm}")
        os.mkdir(directory)
        self._write(
            f"mc0/dimm{dimm}/dimm_location",
            f"channel {channel} slot {slot}\n",
        )
        self._write(f"mc0/dimm{dimm}/size", f"{size}\n")

    def _write(self, relative_path, contents):
        with open(os.path.join(self.sysfs, relative_path), "w",
                  encoding="utf-8") as test_file:
            test_file.write(contents)

    def tearDown(self):
        self.temporary_directory.cleanup()

    def test_parse(self):
        data = MemoryLayout(self.sysfs).parse()

        self.assertEqual(data["layers"], ["mc", "channel", "slot"])
        self.assertEqual(data["num_positions"], [1, 2, 1])
        self.assertEqual(
            data["dimm_sizes"], {(0, 0, 0): 1024, (0, 1, 0): 2048}
        )

    def test_size_mb_may_be_directly_below_memory_controller(self):
        os.remove(os.path.join(self.sysfs, "mc0", "csrow0", "size_mb"))
        self._write("mc0/size_mb", "3072\n")

        data = MemoryLayout(self.sysfs).parse()

        self.assertEqual(
            data["dimm_sizes"], {(0, 0, 0): 1024, (0, 1, 0): 2048}
        )

    def test_missing_size_mb_is_rejected(self):
        os.remove(os.path.join(self.sysfs, "mc0", "csrow0", "size_mb"))

        with self.assertRaisesRegex(RuntimeError, "No memories found"):
            MemoryLayout(self.sysfs).parse()

    def test_display_writes_rendered_table(self):
        output = io.StringIO()

        MemoryLayout(self.sysfs).display(output)

        self.assertEqual(
            output.getvalue(),
            "       +-----------------------+\n"
            "       |          mc0          |\n"
            "       |  channel0 |  channel1 |\n"
            "-------+-----------+-----------+\n"
            "slot0: |     1 GB  |     2 GB  |\n"
            "-------+-----------+-----------+\n",
        )

    def test_size_scaling(self):
        output = io.StringIO()

        MemoryLayout(self.sysfs).display(output)

        self.assertIn("     1 GB  ", output.getvalue())
        self.assertIn("     2 GB  ", output.getvalue())

    def test_display_memory_sizes_in_mb_gb_tb_and_pb(self):
        self._write("mc0/max_location", "channel 3 slot 3\n")
        self._write("mc0/dimm0/size", "512\n")
        self._write("mc0/dimm0/dimm_location", "channel 0 slot 0\n")
        self._write("mc0/dimm1/size", "1024\n")
        self._write("mc0/dimm1/dimm_location", "channel 1 slot 1\n")
        self._add_dimm(2, 2, 1024 * 1024, slot=2)
        self._add_dimm(3, 3, 1024 * 1024 * 1024, slot=3)
        output = io.StringIO()

        MemoryLayout(self.sysfs).display(output)

        rendered_table = output.getvalue()
        lines = rendered_table.splitlines()
        for line in lines:
            self.assertEqual(len(line), len(lines[0]))
        self.assertEqual(
            rendered_table,
            "       +-----------------------------------------------+\n"
            "       |                      mc0                      |\n"
            "       |  channel0 |  channel1 |  channel2 |  channel3 |\n"
            "-------+-----------+-----------+-----------+-----------+\n"
            "slot3: |           |           |           |     1 PB  |\n"
            "slot2: |           |           |     1 TB  |           |\n"
            "-------+-----------+-----------+-----------+-----------+\n"
            "slot1: |           |     1 GB  |           |           |\n"
            "slot0: |   512 MB  |           |           |           |\n"
            "-------+-----------+-----------+-----------+-----------+\n",
        )


if __name__ == "__main__":
    unittest.main()
