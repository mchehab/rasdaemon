# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

import os
import sys
import unittest


PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(PROJECT_DIR, "util"))

from display_table import DisplayTable


class DisplayTableTest(unittest.TestCase):
    def test_none_value_formatter_uses_str(self):
        table = DisplayTable(
            ("column", "row"),
            (2, 1),
            {(0, 0): 10, (1, 0): 20},
            value_formatter=None,
        )

        rendered_table = str(table)

        self.assertIn("row0: |10|20|", rendered_table)

    def test_render_with_two_coordinates_and_multiple_rows(self):
        table = DisplayTable(
            ("group", "row"),
            (2, 3),
            {
                (0, 0): "a000",
                (1, 0): "b000",
                (0, 1): "a001",
                (1, 1): "b001",
                (0, 2): "a002",
                (1, 2): "b002",
            },
        )

        self.assertEqual(
            str(table),
            "      +---------+\n"
            "      |gro |gro |\n"
            "------+----+----+\n"
            "row2: |a002|b002|\n"
            "row1: |a001|b001|\n"
            "row0: |a000|b000|\n"
            "------+----+----+\n",
        )

    def test_render_with_three_coordinates(self):
        table = DisplayTable(
            ("rack", "channel", "row"),
            (2, 2, 2),
            {
                (0, 0, 0): "a000",
                (0, 1, 0): "a010",
                (1, 0, 0): "a100",
                (1, 1, 0): "a110",
                (0, 0, 1): "b000",
                (0, 1, 1): "b010",
                (1, 0, 1): "b100",
                (1, 1, 1): "b110",
            },
        )

        self.assertEqual(
            str(table),
            "      +-------------------+\n"
            "      |  rack0  |  rack1  |\n"
            "      |cha |cha |cha |cha |\n"
            "------+----+----+----+----+\n"
            "row1: |b000|b010|b100|b110|\n"
            "row0: |a000|a010|a100|a110|\n"
            "------+----+----+----+----+\n",
        )

    def test_render_with_four_coordinates(self):
        table = DisplayTable(
            ("socket", "die", "channel", "row"),
            (2, 2, 1, 2),
            {
                (0, 0, 0, 0): "a000",
                (0, 1, 0, 0): "a010",
                (1, 0, 0, 0): "a100",
                (1, 1, 0, 0): "a110",
                (0, 0, 0, 1): "b000",
                (0, 1, 0, 1): "b010",
                (1, 0, 0, 1): "b100",
                (1, 1, 0, 1): "b110",
            },
        )

        self.assertEqual(
            str(table),
            "      +-------------------+\n"
            "      | socket0 | socket1 |\n"
            "      |die |die |die |die |\n"
            "      |cha |cha |cha |cha |\n"
            "------+----+----+----+----+\n"
            "row1: |b000|b010|b100|b110|\n"
            "row0: |a000|a010|a100|a110|\n"
            "------+----+----+----+----+\n",
        )

    def test_render_with_five_coordinates(self):
        table = DisplayTable(
            ("system", "socket", "die", "channel", "row"),
            (1, 2, 2, 1, 2),
            {
                (0, 0, 0, 0, 0): "a000",
                (0, 0, 1, 0, 0): "a010",
                (0, 1, 0, 0, 0): "a100",
                (0, 1, 1, 0, 0): "a110",
                (0, 0, 0, 0, 1): "b000",
                (0, 0, 1, 0, 1): "b010",
                (0, 1, 0, 0, 1): "b100",
                (0, 1, 1, 0, 1): "b110",
            },
        )

        self.assertEqual(
            str(table),
            "      +-------------------+\n"
            "      |      system0      |\n"
            "      | socket0 | socket1 |\n"
            "      |die |die |die |die |\n"
            "      |cha |cha |cha |cha |\n"
            "------+----+----+----+----+\n"
            "row1: |b000|b010|b100|b110|\n"
            "row0: |a000|a010|a100|a110|\n"
            "------+----+----+----+----+\n",
        )

    def test_vertical_alignment_with_sixteen_memory_slots(self):
        table = DisplayTable(
            ("channel", "slot"),
            (1, 16),
            {(0, slot): "16 GB" for slot in range(16)},
        )

        rendered_lines = str(table).splitlines()
        table_border_column = rendered_lines[0].index("+")

        for line in rendered_lines:
            if line.startswith("slot"):
                self.assertEqual(line.index("|"), table_border_column)


if __name__ == "__main__":
    unittest.main()
