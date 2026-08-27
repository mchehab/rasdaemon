# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

"""Parse and display the EDAC memory layout exported through sysfs."""

import os
import re
import sys
from typing import TextIO

from display_table import DisplayTable


class MemoryLayout:
    """Read an EDAC memory hierarchy and display it as an ASCII table."""

    SYSFS_DIR = "/sys/devices/system/edac/mc"

    def __init__(self, sysfs_dir: str = SYSFS_DIR) -> None:
        self.sysfs_dir = os.fspath(sysfs_dir)

    @staticmethod
    def _read(path: str) -> str:
        with open(path, encoding="utf-8") as memory_file:
            return memory_file.read().strip()

    def _format_size(self, value: object) -> str:
        size = int(value)
        units = ("MB", "GB", "TB", "PB")
        scaled = float(size)
        unit = 0
        while scaled >= 1024 and unit < len(units) - 1:
            scaled /= 1024
            unit += 1
        return f"  {int(scaled):4d} {units[unit]}  "

    def parse(self) -> dict[str, object]:
        """Parse the sysfs hierarchy into table layers, bounds, and sizes."""

        layers: list[str] = []
        num_positions: list[int] = []
        memory_size_found = False
        csrow_sizes: dict[tuple[int, int], int] = {}
        dimm_sizes: dict[tuple[int, ...], int] = {}

        paths = []
        for directory, _, filenames in os.walk(self.sysfs_dir):
            for name in filenames:
                paths.append(os.path.join(directory, name))
        paths.sort()

        # Parse max_location first, as File::Find normally encounters it before
        # the DIMM children and the Perl code uses it to initialize the shape.
        paths.sort(key=lambda path: os.path.basename(path) != "max_location")

        for path in paths:
            mc_match = re.search(r"(?:^|/)mc(\d+)(?:/|$)", str(path))
            if not mc_match:
                continue
            mc = int(mc_match.group(1))

            name = os.path.basename(path)

            if name == "max_location":
                tokens = self._read(path).split()

                if len(tokens) % 2:
                    raise ValueError(f"invalid max_location in {path}")

                names = ["mc"] + tokens[::2]
                maxima = [mc] + [int(value) for value in tokens[1::2]]

                if not layers:
                    layers = names
                    num_positions = []
                    for maximum in maxima:
                        num_positions.append(maximum + 1)
                elif names != layers:
                    raise ValueError(f"inconsistent memory layers in {path}")
                else:
                    for position, maximum in enumerate(maxima):
                        found_positions = maximum + 1
                        num_positions[position] = max(
                            num_positions[position],
                            found_positions,
                        )
                continue

            if name == "size_mb":
                memory_size_found = True
                csrow_match = re.search(r"(?:^|/)csrow(\d+)(?:/|$)", str(path))
                if csrow_match:
                    csrow_sizes[(mc, int(csrow_match.group(1)))] = int(
                        self._read(path)
                    )
                continue

            if name != "dimm_location":
                continue

            tokens = self._read(path).split()
            if len(tokens) % 2:
                raise ValueError(f"invalid DIMM location in {path}")
            names = ["mc"] + tokens[::2]
            positions = [mc] + [int(value) for value in tokens[1::2]]

            if not layers:
                layers = names
                num_positions = [1] * len(names)
            elif names != layers:
                raise ValueError(f"inconsistent memory layers in {path}")

            for layer, position in enumerate(positions):
                found_positions = position + 1
                num_positions[layer] = max(
                    num_positions[layer],
                    found_positions,
                )
            size_path = os.path.join(os.path.dirname(path), "size")
            dimm_sizes[tuple(positions)] = int(self._read(size_path))

        # Depending on the driver and kernel version, size_mb may be exposed
        # directly below mcN or below mcN/csrowN.  The Perl implementation
        # accepts either layout (albeit implicitly through its loose csrow
        # regexp), so do not use csrow_sizes alone as the presence check.
        if not memory_size_found:
            raise RuntimeError("No memories found at via edac.")
        if not dimm_sizes:
            raise RuntimeError(
                "EDAC csrow data was found, but the Perl fallback has no "
                "rank data to display"
            )
        return {
            "layers": layers,
            "num_positions": num_positions,
            "dimm_sizes": dimm_sizes,
        }

    def display(self, output: TextIO = sys.stdout) -> None:
        """Parse the memory data, render its table, and display the result."""

        data = self.parse()
        table = DisplayTable(
            data["layers"],
            data["num_positions"],
            data["dimm_sizes"],
            value_formatter=self._format_size,
        )
        output.write(str(table))
