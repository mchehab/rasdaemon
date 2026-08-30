#!/usr/bin/env python3
#
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

"""
This is a poor man's helper tool to generate a roff manual from ras-mc-ctl
argparse help output.

We could use instead argparse-manpage, but not sure how such tool
works those days. There is a report from 2018 mentioning that Debian
packages were broken:
    https://notes.secretsauce.net/notes/2018/10/07_generating-manpages-from-python-and-argparse.html

So, for now, let's just convert the --help content to a simple man page.
"""

import argparse
import datetime
import os
import subprocess
import sys

from textwrap import dedent

def nf_block(text: str) -> str:
    """
    Escape text that will be inserted into a roff nf block
    """

    lines = [".nf"]

    for line in text.splitlines():
        line = line.replace("\\", r"\e")
        if line.startswith((".", "'")):
            line = r"\&" + line
        lines.append(line)

    lines.append(".fi")

    return "\n".join(lines) + "\n"


def rasdaemon_help(program: str, arguments: list[str],
                   env: dict[str, str]) -> str:
    """
    Run rasdaemon help or help command, returning its output.

    Since we're doing this at build time, we need to explicitly set some
    environment variables to be able to run an uninstalled ras-mc-ctl
    code, as otherwise it won't find its libraries.
    """

    cmd = [sys.executable, program, *arguments, "--help"]

    result = subprocess.run(cmd, check=True, capture_output=True,
                            text=True, env=env)
    return result.stdout.strip()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--python-dir", required=True)
    parser.add_argument("--source-dir", required=True)
    parser.add_argument("--sysconf-dir", required=True)
    args = parser.parse_args()

    env = os.environ.copy()
    env["RAS_PYTHON_DIR"] = args.python_dir
    old_path = env.get("PYTHONPATH")
    env["PYTHONPATH"] = os.pathsep.join(
        item for item in (args.python_dir, args.source_dir, old_path) if item
    )

    main_help = rasdaemon_help(args.program, [], env)
    dimm_help = rasdaemon_help(args.program, ["dimm"], env)

    date = datetime.date.today().strftime("%d %B %Y")
    output = dedent(f'''\
        .TH RAS-MC-CTL 8 "{date}" "rasdaemon" "System Administration"
        .SH NAME
        ras-mc-ctl \\- inspect EDAC DIMM information and recorded RAS events
        .SH DESCRIPTION
        This manual is generated from the command-line help of
        .BR ras-mc-ctl .
        .SH GENERAL OPTIONS
    ''')

    output += nf_block(main_help)

    output += '.SH DIMM COMMAND\n'
    output += nf_block(dimm_help)

   # Only add DB help if rasdaemon was compiled with DB support
    try:
        database_help = rasdaemon_help(args.program, ["database"], env)
        output += '.SH DATABASE COMMAND\n'
        output += nf_block(database_help)
    except subprocess.CalledProcessError:
        pass

    output += dedent(f'''\
        .SH FILES
        .I {args.sysconf_dir}/sysconfig/rasdaemon
        .br
        .I {args.sysconf_dir}/ras/dimm_labels.db
        .br
        .I {args.sysconf_dir}/ras/dimm_labels.d
        .br
        .I {args.sysconf_dir}/ras/mainboard
        .SH SEE ALSO
        .BR rasdaemon (1),
        .BR dmidecode (8)
    ''')

    with open(args.output, "w", encoding="utf-8") as fp:
        fp.write(output)


if __name__ == "__main__":
    main()
