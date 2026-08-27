#!/usr/bin/env python3
#
# SPDX-License-Identifier: GPL-2.0-only
# Copyright(c) 2025-2026: Mauro Carvalho Chehab <mchehab@kernel.org>.

import os
import unittest
import sys

PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))

from unittest_helper import TestUnits

if __name__ == "__main__":
    loader = unittest.TestLoader()

    suite = loader.discover(start_dir=PROJECT_DIR, pattern="test*.py")

    TestUnits().run("", suite=suite)
