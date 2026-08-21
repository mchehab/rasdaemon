#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0

# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>


"""
Rasdaemon memory controller admin utility and logs visualizer.

"""

import argparse
import logging
import os
import sys

logger = logging.getLogger(__name__)

PROG = os.path.basename(__file__)


# Check Python version
if sys.version_info < (3, 10):
    print(f"Error: {PROG} requires Python 3.10 or later. Current version:",
          sys.version)
    sys.exit(1)


from ras_env import RasdaemonConfig
from ras_dimm import RasMemoryDimm


def main() -> None:
    """Parse command line arguments."""

    cfg = RasdaemonConfig()

    parser = argparse.ArgumentParser(description=__doc__,
                                     usage="%(prog)s <command> [options]")

    parser.add_argument('--version', "-V", action='version',
                        version=f"%(prog)s {cfg.rasdaemon.version}")

    parser.add_argument("--verbose", "-v",
                        action='count', default=0, help="verbosity level")

    subparsers = parser.add_subparsers(help="Available commands")

    # Each command will pick subparsers at init, adding at the end:
    #           parser.set_defaults(command=self.run_command)

    RasMemoryDimm(PROG, subparsers)

    # Database support is optional, so keep the rest of the utility available
    # when SQLAlchemy or a database-specific driver is not installed.
    try:
        from ras_db import RasDatabaseCommand

        RasDatabaseCommand(PROG, subparsers, cfg)
    except ImportError as error:
        logger.debug("Disabling database command: %s", error)

    args = parser.parse_args()

    if args.verbose:
        level=logging.DEBUG
    else:
        level=logging.INFO

    logging.basicConfig(level=level, format='[%(levelname)s] %(message)s')

    if "func" in args:
        args.func(args)
    else:
        print("Error: no command specified\n", file=sys.stderr)

        parser.print_help(file=sys.stderr)
        print(file=sys.stderr)
        sys.exit(f"Please specify a valid command for {PROG}")


if __name__ == "__main__":
    main()
