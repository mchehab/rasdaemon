# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

import re
import unittest

from unittest_helper import TestUnits


class TestUnitsKeywordTest(unittest.TestCase):
    def test_keyword_accepts_multiple_patterns(self):
        args = TestUnits().parse_args().parse_args([
            "-k", "FirstTests", "test_second"
        ])

        self.assertEqual(args.keyword, ["FirstTests", "test_second"])

    def test_keyword_matches_method_concrete_and_base_class(self):
        class SharedTests:
            pass

        class ConcreteTests(SharedTests, unittest.TestCase):
            def test_example(self):
                pass

        test = ConcreteTests("test_example")

        for keyword in ("test_example", "ConcreteTests", "SharedTests"):
            with self.subTest(keyword=keyword):
                self.assertTrue(
                    TestUnits._matches_keyword(test, re.compile(keyword))
                )

        self.assertFalse(
            TestUnits._matches_keyword(test, re.compile("UnrelatedTests"))
        )


if __name__ == "__main__":
    TestUnits().run(__file__)
