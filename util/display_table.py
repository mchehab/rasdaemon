#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0

# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>


"""
Convert data from the memory layout as detected with EDAC into a
string with an ASCII table that will be displayed like this::

                   +-----------------------------------------------+
                   |                      mc0                      |
                   |  channel0 |  channel1 |  channel2 |  channel3 |
            -------+-----------+-----------+-----------+-----------+
            slot3: |     0 MB  |     0 MB  |     0 MB  |     1 PB  |
            slot2: |     0 MB  |     0 MB  |     2 TB  |     0 MB  |
            -------+-----------+-----------+-----------+-----------+
            slot1: |     0 MB  |   128 GB  |     0 MB  |     0 MB  |
            slot0: |   512 MB  |     0 MB  |     0 MB  |     0 MB  |
            -------+-----------+-----------+-----------+-----------+

The code itself is designed to generic enough to be used with other
types of data, and it may use a callable function to convert the format
of each object, for instance to convert then from MG to GB, if the number
is too big.
"""

from typing import Callable, Optional


class DisplayTable:
    """
    Produce an ASCII artwork string containing a table to be displayed with
    a complex format.

    The output is produced without any memory-specific code inside it.
    To do that, each value to be inserted has a multi-dimensional
    coordinate associated with it, represented by a tuple.

    As the goal is to display memory, a callback is added to allow
    scaling it into MB, GB, PB... as needed, while keeping the code
    generic, e.g. to generate a string containing the memory layout on
    an easy representation of it, like:

            "       +-----------------------------------------------+\n"
            "       |                      mc0                      |\n"
            "       |  channel0 |  channel1 |  channel2 |  channel3 |\n"
            "-------+-----------+-----------+-----------+-----------+\n"
            "slot3: |     0 MB  |     0 MB  |     0 MB  |     1 PB  |\n"
            "slot2: |     0 MB  |     0 MB  |     1 TB  |     0 MB  |\n"
            "-------+-----------+-----------+-----------+-----------+\n"
            "slot1: |     0 MB  |     1 GB  |     0 MB  |     0 MB  |\n"
            "slot0: |   512 MB  |     0 MB  |     0 MB  |     0 MB  |\n"
            "-------+-----------+-----------+-----------+-----------+\n",

    ``layers`` describe the names of each coordinate:
    - The first value is mapped as the main X coordinate axis;
    - Intermediate values are also mapped as subdivisions at X axis;
    - The last value is mapped as the Y axis.

    For example::

        ("mc", "channel", "slot")

    would create a table like this:

    ``num_positions`` is a tuple that contains the size of each dimension
    at the table. For instance::

        (2, 4, 8)

    with the layers example above would mean 2 memory controllers,
    4 channels, each one with 8 slots.

    ``values`` contains a dict that maps a table coordinate with the value
    to be added on its location. For instance::

        {
            (0, 0, 0): "8 GB",      # mc 0, channel 0, slot 0
            (0, 1, 0): "256 MB",    # mc 0, channel 1, slot 0
            (1, 0, 0): "128 GB",    # mc 1, channel 0, slot 0
        }

    If a coordinate is missing, an empty value will be added on its cell.

    ``value_formatter`` contains an optional callable to convert each value.
    It is useful to add, for instance, a logic to avoid things like
    32764MB, changing its scale to GB and displaying it as 32GB.

    ```empty_value``` is the value to be placed at the cell when there's no
    value associated with the coordinate. If not set, an empty cell will be
    displayed.
    """

    def __init__(self,
                 layers: list[str],
                 num_positions: list[int],
                 values: dict[tuple[int, ...], object],
                 value_formatter: Optional[Callable[[object], str]] = None,
                 empty_value: object = None) -> None:
        """
        Check if the data is consistend and initialize class values.

        If the class is initialized with bad values, it raises ValueError.
        """

        if len(layers) != len(num_positions):
            raise ValueError("layers and num_positions must have equal lengths")
        if not layers:
            raise ValueError("at least one table layer is required")
        if any(positions < 1 for positions in num_positions):
            raise ValueError("each layer must contain at least one position")

        self.layers = tuple(layers)
        self.num_positions = tuple(num_positions)
        self.values = values
        self.value_formatter = value_formatter
        self.empty_value = empty_value

        self.coordinates = self._gen_coordinates(num_positions)

    @staticmethod
    def _gen_coordinates(num_positions: list[int]) -> list[tuple[int, ...]]:
        """
        Create a list containing a cell for each possible coordinate
        based on num_positions. So, for::

            num_positions = [ 3, 2, 4]

        it would produce a list with the Cartesian map of all cells::

              [
                  (0, 0, 0), (0, 0, 1), (0, 0, 2), (0, 0, 3),
                  (0, 1, 0), (0, 1, 1), (0, 1, 2), (0, 1, 3),
                  (1, 0, 0), (1, 0, 1), (1, 0, 2), (1, 0, 3),
                  (1, 1, 0), (1, 1, 1), (1, 1, 2), (1, 1, 3),
                  (2, 0, 0), (2, 0, 1), (2, 0, 2), (2, 0, 3),
                  (2, 1, 0), (2, 1, 1), (2, 1, 2), (2, 1, 3),
              ]

        """

        coordinates = [()]

        for positions in num_positions:
            expanded_coordinates = []

            for coordinate in coordinates:
                for position in range(positions):
                    expanded_coordinates.append(coordinate + (position,))

            coordinates = expanded_coordinates

        return coordinates

    def _formatted_cells(self) -> tuple[dict[tuple[int, ...], str], int]:
        cells = {}
        width = 0

        for coordinate in self.coordinates:
            value = self.values.get(coordinate, self.empty_value)

            if value:
                if self.value_formatter:
                    formatted_value = self.value_formatter(value)
                    cells[coordinate] = str(formatted_value)
                else:
                    cells[coordinate] = str(value)
            else:
                cells[coordinate] = ""

            width = max(width, len(cells[coordinate]))

        return cells, width

    def _header(self, layer: int, total_columns: int, cell_width: int) -> str:
        item_count = 1
        for positions in self.num_positions[: layer + 1]:
            item_count *= positions

        span = total_columns // item_count
        field_width = span * (cell_width + 1) - 1
        fields = []

        for index in range(item_count):
            label = f"{self.layers[layer]}{index % self.num_positions[layer]}"
            label = label[: max(field_width - 1, 0)]
            fields.append(label.center(field_width))

        return "|" + "|".join(fields) + "|"

    def __str__(self) -> str:
        """Return the complete table, including its trailing newline."""

        cells, cell_width = self._formatted_cells()
        column_coordinates = self._gen_coordinates(self.num_positions[:-1])
        total_columns = len(column_coordinates)

        row_label_width = len(f"{self.layers[-1]}{self.num_positions[-1] - 1}: ")
        table_width = total_columns * (cell_width + 1) + 1
        separator = "-" * row_label_width + "+"

        for column in column_coordinates:
            separator += "-" * cell_width + "+"

        top_separator = " " * row_label_width + "+" + "-" * (table_width - 2) + "+"

        lines = [top_separator]
        for layer in range(len(self.layers) - 1):
            lines.append(
                " " * row_label_width + self._header(layer, total_columns, cell_width)
            )

        last_maximum = self.num_positions[-1] - 1
        even_row_count = bool(last_maximum % 2)

        if not even_row_count:
            lines.append(separator)

        for row in range(last_maximum, -1, -1):
            if even_row_count and (last_maximum - row) % 2 == 0:
                lines.append(separator)
            rendered_cells = ""
            for column in column_coordinates:
                coordinate = column + (row,)
                rendered_cells += cells[coordinate].ljust(cell_width)
                rendered_cells += "|"
            row_label = f"{self.layers[-1]}{row}: ".ljust(row_label_width)
            lines.append(f"{row_label}|{rendered_cells}")

        lines.append(separator)
        return "\n".join(lines) + "\n"
