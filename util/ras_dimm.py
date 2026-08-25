#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0

# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

"""DIMM commands."""

import glob
import logging
import os
import re
import subprocess
import sys
import time
from shutil import which
from typing import Any

from mem_layout import MemoryLayout


logger = logging.getLogger(__name__)


class RasMemoryDimm:
    """Implement the DIMM-related commands from ras-mc-ctl."""

    SYSFS_DIR = "/sys/devices/system/edac/mc"
    DMI_DIR = "/sys/class/dmi/id"
    DEFAULT_LABEL_DB = "/etc/ras/dimm_labels.db"
    DEFAULT_LABEL_DIR = "/etc/ras/dimm_labels.d"

    def __init__(self, prog, subparsers):
        self.prog = prog
        self.mainboard_vendor = "unknown"
        self.mainboard_model = "unknown"
        self.product_vendor = "unknown"
        self.product_name = "unknown"
        self.label_db = self.DEFAULT_LABEL_DB
        self.label_dir = self.DEFAULT_LABEL_DIR
        self.delay = 0

        parser = subparsers.add_parser(
            "dimm", aliases=["mem"],
            description="Inspect EDAC DIMMs and manage their labels.",
            help="Inspect EDAC DIMMs and manage their labels.",
        )
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
        parser.add_argument("--delay", "-d", type=float, default=0,
                            help="Delay DELAY seconds before writing DIMM labels.")
        parser.add_argument("--labeldb", "-L",
                            help="Load label database from file DB.")
        parser.add_argument("--layout", "-l", action="store_true",
                            help="Display the memory layout.")
        parser.add_argument("--error-count", "-e", action="store_true",
                            help="Display corrected and uncorrected DIMM error counts.")
        parser.add_argument("--per-rank", "-P", action="store_true",
                            help="With --error-count, show every rank separately.")
        self.parser = parser
        parser.set_defaults(func=self.run)

    def run(self, config:Any, args: Any) -> None:
        self.delay = args.delay
        if args.labeldb:
            self.label_db = args.labeldb
        if args.per_rank and not args.error_count:
            self.parser.error("Only use --per-rank with --error-count")

        needs_board = (args.mainboard or args.dmidecode or args.print_labels or
                       args.register_labels)
        if needs_board:
            self.get_mainboard_info(args.vendor, args.model, args.dmidecode)

        actions = 0
        if args.mainboard or args.dmidecode:
            print(f"{self.prog}: mainboard: {self.mainboard_vendor} "
                  f"model {self.mainboard_model}\n")
            actions += 1
        if args.print_labels:
            self.print_dimm_labels()
            actions += 1
        if args.register_labels:
            self.register_dimm_labels()
            actions += 1
        if args.layout:
            self.display_memory_layout()
            actions += 1
        if args.error_count:
            self.display_error_count(args.per_rank)
            actions += 1
        if args.status:
            self.print_status()
            actions += 1
        if args.guess_labels:
            self.guess_dimm_label()
            actions += 1
        if not actions:
            logger.error("Missing argument")

    @staticmethod
    def _read_text(path):
        try:
            with open(path, encoding="utf-8") as source:
                value = source.read().strip()
                return value or None
        except OSError:
            return None

    def _read_product_info(self):
        self.product_vendor = (self._read_text(
            os.path.join(self.DMI_DIR, "product_vendor")) or
            self.mainboard_vendor)
        self.product_name = (self._read_text(
            os.path.join(self.DMI_DIR, "product_name")) or "unknown")

    def get_mainboard_info(self, vendor="unknown", model="unknown",
                           force_dmidecode=False):
        """Get board data from overrides, sysfs, then dmidecode, like Perl."""
        supplied_vendor = vendor not in (None, "", "unknown")
        supplied_model = model not in (None, "", "unknown")

        if supplied_vendor and supplied_model:
            self.mainboard_vendor, self.mainboard_model = vendor, model
            self._read_product_info()
            return True

        board_vendor = board_model = None
        if not force_dmidecode:
            board_vendor = self._read_text(os.path.join(self.DMI_DIR, "board_vendor"))
            board_model = self._read_text(os.path.join(self.DMI_DIR, "board_name"))

        if not (board_vendor and board_model):
            decoded = self._run_dmidecode()
            if decoded is not None:
                board_vendor, board_model = self._parse_board_dmidecode(decoded)

        self.mainboard_vendor = vendor if supplied_vendor else (board_vendor or "unknown")
        self.mainboard_model = model if supplied_model else (board_model or "unknown")
        self._read_product_info()
        return board_vendor is not None and board_model is not None

    def _run_dmidecode(self):
        dmidecode = which("dmidecode")
        if not dmidecode:
            print(f"{self.prog}: Can't run dmidecode: program not found.",
                  file=sys.stderr)
            return None
        if os.geteuid() != 0:
            print(f"{self.prog}: dmidecode requires root permissions; "
                  f"please run this command using sudo.", file=sys.stderr)
            return None
        try:
            result = subprocess.run([dmidecode], capture_output=True, text=True,
                                    timeout=5, check=False)
        except (OSError, subprocess.TimeoutExpired) as error:
            print(f"{self.prog}: failed to run {dmidecode}: {error}",
                  file=sys.stderr)
            return None
        if result.returncode:
            detail = result.stderr.strip()
            suffix = f": {detail}" if detail else ""
            print(f"{self.prog}: failed to run {dmidecode}{suffix}",
                  file=sys.stderr)
            return None
        return result.stdout

    @staticmethod
    def _parse_board_dmidecode(output):
        board_vendor = board_model = system_vendor = system_model = None
        section = None
        section_indent = -1
        for line in output.splitlines():
            heading = re.match(r"^(\s*)(board|base board|system) information\s*$",
                               line, re.I)
            if heading:
                section_indent = len(heading.group(1))
                section = heading.group(2).lower()
                continue
            if section is None:
                continue
            if not line.strip():
                section = None
                continue
            indent = len(line) - len(line.lstrip())
            if indent <= section_indent:
                section = None
                continue
            match = re.match(r"^\s*(manufacturer|vendor|product(?: name)?)\s*:\s*(.*?)\s*$",
                             line, re.I)
            if not match or not match.group(2):
                continue
            key, value = match.group(1).lower(), match.group(2)
            if section == "system":
                if key in ("manufacturer", "vendor"):
                    system_vendor = value
                else:
                    system_model = value
            else:
                if key in ("manufacturer", "vendor"):
                    board_vendor = value
                else:
                    board_model = value
            if board_vendor and board_model:
                break
        return (board_vendor or system_vendor, board_model or system_model)

    def print_status(self):
        """Check if an EDAC kernel module is loaded."""
        try:
            status = any("edac" in entry.name
                         for entry in os.scandir("/sys/module"))
        except OSError:
            status = False
        print(f"{self.prog}: drivers {'are' if status else 'not'} loaded.")
        return status

    def guess_dimm_label(self):
        """Print Locator/Bank Locator pairs from DMI memory devices."""
        output = self._run_dmidecode()
        if output is None:
            return False
        in_device = False
        label = address = None
        for line in output.splitlines() + [""]:
            if re.match(r"^\s*memory device\s*$", line, re.I):
                in_device, label, address = True, None, None
                continue
            if not in_device:
                continue
            if not line.strip():
                if label and address:
                    print(f"memory stick '{label}' is located at '{address}'")
                in_device = False
                continue
            match = re.match(r"^\s*(locator|bank locator)\s*:\s*(.*\S)\s*$",
                             line, re.I)
            if match:
                if match.group(1).lower() == "locator":
                    label = match.group(2)
                else:
                    address = match.group(2)
            if label and address:
                print(f"memory stick '{label}' is located at '{address}'")
                in_device = False
        return True

    @staticmethod
    def _parse_label_file(path, board_labels, product_labels):
        vendor = ""
        targets = []
        target_map = board_labels
        layer_count = None
        with open(path, encoding="utf-8") as label_file:
            for line_number, raw_line in enumerate(label_file):
                line = raw_line.strip()
                if not line or line.startswith("#"):
                    continue
                match = re.match(r"vendor\s*:\s*(.*\S)\s*$", line, re.I)
                if match:
                    vendor = match.group(1).lower()
                    targets, layer_count = [], None
                    continue
                match = re.match(r"(model|board|product)\s*:\s*(.*)$", line, re.I)
                if match:
                    if not vendor:
                        raise ValueError(f"{path}: line {line_number}: target without vendor")
                    target_map = (product_labels if match.group(1).lower() == "product"
                                  else board_labels)
                    targets = [item.strip().lower() for item in
                               re.split(r"[,;]+", match.group(2)) if item.strip()]
                    layer_count = None
                    continue
                for item in line.split(";"):
                    match = re.match(r"^\s*(.*?)\s*:\s*(.*?)\s*$", item)
                    if not match:
                        continue
                    label, positions = match.groups()
                    for position in re.split(r"[, ]+", positions):
                        if not position:
                            continue
                        if not re.fullmatch(r"\d+(?:[.:]\d+)+", position):
                            logger.error('%s: %d: Invalid syntax, ignoring: "%s"',
                                         path, line_number, raw_line.rstrip())
                            continue
                        values = tuple(int(value) for value in re.split(r"[.:]", position))
                        layers = len(values) - 1
                        if layers > 3:
                            raise ValueError("Only up to 3 layers are currently "
                                             f"supported on label db \"{path}\"")
                        if layer_count is None:
                            layer_count = layers
                        elif layer_count != layers:
                            raise ValueError("Inconsistent number of layers at "
                                             f"label db \"{path}\"")
                        padded = values + (0,) * (4 - len(values))
                        for target in targets:
                            entry = target_map.setdefault((vendor, target),
                                                          {"layers": layers, "labels": {}})
                            entry["labels"][padded] = label

    def _read_dimm_labels(self):
        boards, products = {}, {}
        paths = [self.label_db]
        if self.label_dir:
            paths.extend(sorted(glob.glob(os.path.join(self.label_dir, "*"))))
        for path in paths:
            if os.path.isfile(path) and os.access(path, os.R_OK):
                self._parse_label_file(path, boards, products)
        board = boards.get((self.mainboard_vendor.lower(),
                            self.mainboard_model.lower()))
        product = products.get((self.product_vendor.lower(),
                                self.product_name.lower()))
        return board or product

    def _parse_dimm_nodes(self):
        nodes = {}
        for path in glob.glob(os.path.join(self.SYSFS_DIR, "mc*", "**",
                                           "dimm_location"), recursive=True):
            mc_match = re.search(r"(?:^|/)mc(\d+)(?:/|$)", path)
            location = self._read_text(path)
            if not mc_match or not location:
                continue
            tokens = location.split()
            if len(tokens) % 2:
                continue
            position = (int(mc_match.group(1)),) + tuple(
                int(value) for value in tokens[1::2])
            nodes[position] = {
                "path": os.path.join(os.path.dirname(path), "dimm_label"),
                "location": location,
            }
        return nodes

    def print_dimm_labels(self):
        entry = self._read_dimm_labels()
        if not entry:
            print(f"{self.prog}: No dimm labels for {self.mainboard_vendor} "
                  f"model {self.mainboard_model}\n", file=sys.stderr)
            return False
        nodes = self._parse_dimm_nodes()
        print(f"{'LOCATION':35s} {'CONFIGURED LABEL':20s} {'SYSFS CONTENTS':20s}")
        layers = entry["layers"]
        for position, label in sorted(entry["labels"].items()):
            key = position[:layers + 1]
            node = nodes.get(key)
            if not node:
                location, current = "", f"{':'.join(map(str, key))} missing"
            elif not os.path.isfile(node["path"]):
                location, current = f"{':'.join(map(str, key))} missing", "label missing"
            else:
                current = self._read_text(node["path"]) or ""
                location = f"mc{key[0]} {node['location']}"
            print(f"{location:35s} {label:20s} {current:20s}")
        print()
        return True

    def register_dimm_labels(self):
        entry = self._read_dimm_labels()
        if not entry:
            print(f"{self.prog}: No dimm labels for {self.mainboard_vendor} "
                  f"model {self.mainboard_model}\n", file=sys.stderr)
            return False
        nodes = self._parse_dimm_nodes()
        time.sleep(self.delay)
        layers = entry["layers"]
        for position, label in sorted(entry["labels"].items()):
            node = nodes.get(position[:layers + 1])
            if not node or not os.path.isfile(node["path"]):
                continue
            try:
                with open(node["path"], "w", encoding="utf-8") as output:
                    output.write(label)
            except OSError as error:
                print(f"{self.prog}: Unable to write {node['path']}: {error}",
                      file=sys.stderr)
        return True

    def display_memory_layout(self):
        MemoryLayout(self.SYSFS_DIR).display()

    def display_error_count(self, per_rank=False):
        """Display EDAC corrected and uncorrected error counters from sysfs."""

        nodes = self._parse_dimm_nodes()
        if not nodes:
            logger.error("No DIMMs found in /sys or new sysfs EDAC interface not found.")
            return False

        rows = []
        for position, node in sorted(nodes.items()):
            directory = os.path.dirname(node["path"])
            label = self._read_text(node["path"]) or "unknown"
            ce_count = self._read_text(os.path.join(directory, "dimm_ce_count"))
            ue_count = self._read_text(os.path.join(directory, "dimm_ue_count"))
            try:
                ce_count = int(ce_count)
                ue_count = int(ue_count)
            except (TypeError, ValueError):
                logger.error("Missing or invalid EDAC error counters for %s", label)
                return False
            rows.append((label, node["location"], ce_count, ue_count))

        if per_rank:
            labels = [f"{label} ({location})" for label, location, _, _ in rows]
            width = max(len(label) for label in labels)
            print(f"{'Label':{width}}\tCE\tUE")
            for label, (_, _, ce_count, ue_count) in zip(labels, rows):
                print(f"{label:{width}}\t{ce_count}\t{ue_count}")
            return True

        counts = {}
        for label, _, ce_count, ue_count in rows:
            total = counts.setdefault(label, [0, 0])
            total[0] += ce_count
            total[1] += ue_count

        width = max(len(label) for label in counts)
        print(f"{'Label':{width}}\tCE\tUE")
        for label in sorted(counts):
            ce_count, ue_count = counts[label]
            print(f"{label:{width}}\t{ce_count}\t{ue_count}")
        return True
