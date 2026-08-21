#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0

# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>


import sys
import re
import os
import logging
import argparse
import time
import glob
import shlex
import subprocess

from typing import Optional, List
from shutil import which

from mem_layout import MemoryLayout

"""
DIMM commands
"""

logger = logging.getLogger(__name__)




class RasMemoryDimm:
    def __init__(self, prog, subparsers):
        """Initialize the memory controller class and add subparser"""

        self.prog = prog
        self.mainboard_vendor = "unknown"
        self.mainboard_model = "unknown"

        parser = subparsers.add_parser("dimm", description=__doc__)

        parser.add_argument("--mainboard", "-m", action="store_true",
                            help="Print mainboard vendor and model for this hardware.")

        parser.add_argument("--dmidecode", "-D", action="store_true",
                            help="Force dmidecode to parse mainboard vendor and model.")

        parser.add_argument("--vendor", "-V", default="unknown",
                            help="Force hardware vendor (e.g., Dell, HP, Lenovo)")
        parser.add_argument("--model", "-M", default="unknown",
                            help="Force hardware model (e.g., R740, ProLiant DL380)")

        parser.add_argument("--status", "-s", action="store_true",
                            help="Print status of EDAC drivers.")
        parser.add_argument("--print-labels", "-p", action="store_true",
                            help="Print Motherboard DIMM labels to stdout.")
        parser.add_argument("--guess-labels", "-g", action="store_true",
                            help="Print DMI labels, when bank locator is available.")
        parser.add_argument("--register-labels", "-r", action="store_true",
                            help="Load Motherboard DIMM labels into EDAC driver.")
        parser.add_argument("--delay", "-d", type=int,
                            help="Delay DELAY seconds before writing DIMM labels.")
        parser.add_argument("--labeldb", "-L",
                            help="Load label database from file DB.")
        parser.add_argument("--layout", "-l", action="store_true",
                            help="Display the memory layout.")

        # Parse run command to main parser
        parser.set_defaults(func=self.run)


    def run(self, args):
        nargs = 0
        if args.mainboard or args.dmidecode:
            self.get_mainboard_info(args.vendor, args.model, args.dmidecode)
            print(f"{self.prog}: mainboard: {self.mainboard_vendor} model {self.mainboard_model}\n")
            nargs += 1

        if args.print_labels:
            self.print_dimm_labels()
            nargs += 1

        if args.register_labels:
            self.register_dimm_labels()
            nargs += 1

        if args.layout:
            self.display_memory_layout()
            nargs += 1

        if args.status:
            return self.print_status()

        if args.guess_labels:
            self.guess_dimm_label()
            nargs += 1

        if not nargs:
            logger.error("Missing argument")

    def get_mainboard_info(self, vendor, model, force_dmidecode):
        """Get mainboard vendor and model information."""

        # Try first via sysfs
        if not force_dmidecode:
            try:
                with open("/sys/class/dmi/id/board_vendor", encoding="utf-8") as f:
                    vendor = f.read().strip()
                    self.mainboard_vendor = vendor

            except (FileNotFoundError, IOError):
                pass

            try:
                with open("/sys/class/dmi/id/board_name", encoding="utf-8") as f:
                    model = f.read().strip()
                    self.mainboard_model = model

            except (FileNotFoundError, IOError):
                pass

            if (self.mainboard_vendor != "unknown" and
                self. mainboard_model != "unknown"):

                return

        # Fall back to dmidecode if not forced and sysfs is not there

        dmidecode = which("dmidecode")
        if not dmidecode:
            print("Can't run dmidecode: program not found.")
            return

        print(f"Running: sudo {dmidecode}")
        try:
            result = subprocess.run(["sudo", dmidecode],
                                    capture_output=True, text=True, timeout=5)
            if result.returncode != 0:
                return
        except subprocess.CalledProcessError as e:
            print(f"Fail to run {dmidecode}: {repr(e)}")
            return

        # Parse dmidecode output

        vendor = ""
        model = ""
        system_vendor = ""
        system_model = ""
        in_board_or_system = False

        indent = None
        info_type = None

        for line in result.stdout.split('\n'):
            match = re.match(r"(\s*)(board|base board|system) information",
                                line, re.I)
            if match:
                in_board_or_system = True
                indent = match.group(1)
                info_type = match.group(2)
                continue

            if in_board_or_system:
                match = re.match(r"(\s*)", line)
                if match:
                    if len(match.group(1)) == len(indent):
                        in_board_or_system = False
                        continue

                if info_type.lower() == "system":
                    match = re.search(r'(?:manufacturer|vendor):(.*)', line, re.I)
                    if match:
                        system_vendor = match.group(1).strip()

                    match = re.search(r'product(?: name):(.*)', line, re.I)
                    if match:
                        system_model = match.group(1).strip()
                else:
                    match = re.search(r'(?:manufacturer|vendor):(.*)', line, re.I)
                    if match:
                        vendor = match.group(1).strip()

                    match = re.search(r'product(?: name):(.*)', line, re.IGNORECASE)
                    if match:
                        model = match.group(1).strip()

                if vendor and model:
                    break

        if not vendor:
            vendor = system_vendor

        if not model:
            model = system_model

        self.mainboard_vendor = vendor
        self.mainboard_model = model

        return

    def print_status(self):
        """Check if EDAC drivers are loaded."""

        status = False

        try:
            # Iterate over the entries in /sys/module
            with os.scandir("/sys/module") as entries:
                for entry in entries:
                    # Look for a name that contains "edac"
                    if "edac" in entry.name:
                        status = True
                        break
        except FileNotFoundError:
            pass

        if status:
            print(f"{self.prog}: drivers are loaded.\n", file=sys.stderr)
        else:
            print(f"{self.prog}: drivers not loaded.\n", file=sys.stderr)

        return status

    def print_dimm_labels(self):
        """Print DIMM labels from the label database."""

        db = RasDatabase()
        labels = db.read_dimm_labels()
        if not labels:
            print(f"{self.prog}: No dimm labels found for {self.mainboard_vendor} model {self.mainboard_model}\n",
                  file=sys.stderr)
            return

        for mc, top in sorted(labels.items()):
            for mid, low in sorted(top.items()):
                label = labels[mc][top][mid][low]
                print(f"{mc:35s} {label:20s}")

    def guess_dimm_label(self):
        """Guess DIMM labels from dmidecode."""
        dmidecode = which("dmidecode")
        if not dmidecode:
            print(f"{self.prog}: Can't guess DIMM labels: dmidecode not found in PATH\n",
                  file=sys.stderr)
            return

        try:
            with open(dmidecode, "r") as proc:
                # Run dmidecode via subprocess
                import subprocess
                result = subprocess.run([dmidecode],
                                        capture_output=True, text=True, timeout=5)
                if result.returncode != 0:
                    print(f"{self.prog}: failed to run dmidecode: {result.stderr}\n",
                          file=sys.stderr)
                    return

                for line in result.stdout.split('\n'):
                    line_lower = line.lower()
                    if 'memory device' in line_lower:
                        label = None
                        addr = None
                        while True:
                            nxt = next(result.stdout, '')
                            if 'locator' in nxt.lower():
                                if 'locator' in nxt.lower():
                                    label = nxt.split(':', 1)[1].strip() if ':' in nxt else None
                                elif 'bank locator' in nxt.lower():
                                    addr = nxt.split(':', 1)[1].strip() if ':' in nxt else None
                            if label and addr:
                                print(f"memory stick '{label}' is located at '{addr}'\n")
                                break
                            if '\n' in line_lower:
                                break
        except Exception as e:
            print(f"{self.prog}: Error guessing DIMM labels: {e}\n", file=sys.stderr)

    def register_dimm_labels(self):
        """Register DIMM labels from the label database."""

        db = RasDatabase()
        labels = db.read_dimm_labels()
        if not labels:
            print(f"{self.prog}: No dimm labels for {self.mainboard_vendor} "
                  f"model {self.mainboard_model}\n", file=sys.stderr)
            return False

        if args.delay:
            time.sleep(int(args.delay))

        for mc, top in sorted(labels.items()):
            for mid, low in sorted(top.items()):
                label = labels[mc][top][mid][low]
                sysfs_path = self._get_sysfs_path(mc, top, mid, low)
                if os.path.isfile(sysfs_path):
                    try:
                        with open(sysfs_path, "w") as f:
                            f.write(label)
                    except IOError as e:
                        print(f"{self.prog}: Unable to write {sysfs_path}: {e}\n",
                              file=sys.stderr)

        return True

    def _get_sysfs_path(self, mc, top, mid, low):
        """Get the sysfs path for a DIMM node."""
        if low is not None:
            return f"/sys/devices/system/edac/mc/{mc}/{top}/{mid}/{low}"
        elif mid is not None:
            return f"/sys/devices/system/edac/mc/{mc}/{top}/{mid}"
        else:
            return f"/sys/devices/system/edac/mc/{mc}/{top}"

    def display_memory_layout(self):
        """Display the memory layout."""

        layout = MemoryLayout()
        layout.display()
