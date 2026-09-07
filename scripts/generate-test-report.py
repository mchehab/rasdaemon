#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>

"""Create GitHub Pages test reports and numeric status badges."""

import argparse
import html
import os
import re
import shutil
import subprocess
import sys

from textwrap import dedent

STATIC_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "docs", "static")
test_failed = False

REPORT_HEADER = dedent("""\
    <!doctype html>
    <html lang="en"><head><meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>{title}</title>
    <link rel="stylesheet" href="results.css">
    <script src="results.js" defer></script>
    </head><body><h1>{title}</h1>
    <p><a href="../index.html">Documentation</a> | <a href="index.html">All test results</a></p>
    <div class="toolbar"><label>Filter tests or status: <input id="filter" placeholder="e.g. FAIL, database, queue"></label></div>
    <button type="button" id="theme" class="theme-toggle" title="Change color theme" aria-label="Change color theme">◐ Auto</button>
    {body}</body></html>
""")

INDEX_CONTENT = dedent("""\
    <h2>Detailed results</h2><ul>
    <li><a href="rasdaemon.html">rasdaemon CMocka tests</a></li>
    <li><a href="ras-mc-ctl.html">ras-mc-ctl Python tests</a></li>
    </ul>
""")

BADGE = dedent("""\
    <svg xmlns="http://www.w3.org/2000/svg" width="28" height="20">
    <rect width="28" height="20" fill="#{color}"/>
    <text x="7" y="14" fill="#fff" font-family="Verdana" font-size="11">{value}</text>
    </svg>
""")

TABLE_HEADER = dedent("""\
    <h2>{title}</h2>
    <table id="results"><thead><tr><th>Test group</th><th>Test</th><th>Result</th></tr></thead><tbody>
""")

TABLE_GROUP_ROW = dedent("""\
    <tr><td rowspan="{rowspan}">{group}</td><td>{name}</td><td class="{status}">{status}</td></tr>
""")

TABLE_ROW = dedent("""\
    <tr><td>{name}</td><td class="{status}">{status}</td></tr>
""")

TABLE_FOOTER = dedent("""\
    </tbody></table>
""")


def table(title, results):
    rows = ""
    group = None
    rowspan = 0

    for test_group, name, status in results:
        name = name.removeprefix("test_").replace("_", " ")
        name = name.strip()

        if test_group != group:
            group = test_group
            rowspan = sum(item[0] == group for item in results)
            rows += TABLE_GROUP_ROW.format(group=html.escape(group), name=html.escape(name),
                                           status=status, rowspan=rowspan) + "\n"
        else:
            rows += TABLE_ROW.format(name=html.escape(name), status=status) + "\n"

    return TABLE_HEADER.format(title=html.escape(title)) + rows + TABLE_FOOTER


def write_report(output, filename, title, results):
    with open(os.path.join(output, filename), "w", encoding="utf-8") as f:
        f.write(REPORT_HEADER.format(title=html.escape(title), body=table(title, results)))

def run_cmd(cmd):
    global test_failed

    try:
        result = subprocess.run(cmd, capture_output=True, text=True)
    except OSError as error:
        print(f"Unable to run {' '.join(cmd)}: {error}")
        test_failed = True
        return ""

    output = result.stdout + result.stderr

    print(output)

    if result.returncode:
        print(f"Command {' '.join(cmd)} failed with exit code {result.returncode}")
        test_failed = True

    return output

DB_TESTS = {
    "MysqlRasDatabaseTest": "mysql",
    "MysqlUrlContractTest": "mysql",
    "PostgresqlRasDatabaseTest": "postgres",
    "PostgresqlUrlContractTest": "postgres",
    "SqliteRasDatabaseTest": "sqlite",
    "TimestampConversionTest": "timestamps",
}

MODULES = {
    "test_ras_db": "database",
    "test_ras_db_decode": "database: decode",
    "test_ras_dimm": "dimm",
    "test_ras_env": "environment",
}

def unittest_group(group):
    module, name, _ = group.split(".", 2)

    if module == "test_ras_db":
        subgroup = DB_TESTS.get(name)
        if subgroup:
            return "database: " + subgroup

    group = MODULES.get(module)
    if group:
        return group

    return module.removeprefix("test_").replace("_", " ")

def parse_tap(build_dir):
    results = []
    tests = []
    pattern = re.compile(r"^(not )?ok \d+ - (.*?)(?:\s+#.*)?$")
    group_pattern = re.compile(r"^# (?:not )?ok - (.*)$")

    out = run_cmd([f"{build_dir}/unittest", "-o", "tap"])

    for line in out.splitlines():
        match = pattern.match(line)
        if match:
            tests.append((match.group(2), "FAIL" if match.group(1) else "PASS"))
            continue

        match = group_pattern.match(line)
        if match:
            results += [(match.group(1), name, status) for name, status in tests]
            tests = []

    results += [("Ungrouped tests", name, status) for name, status in tests]

    return results

def parse_unittest():
    results = []
    pattern = re.compile(r"^(test_[^ ]+) \(([^)]+)\) \.\.\. (ok|FAIL|ERROR|skipped.*)$")

    out = run_cmd(["tests/run.py", "-vvv"])

    for line in out.splitlines():
        match = pattern.match(line)
        if not match:
            continue

        status = match.group(3)
        if status == "ok":
            status = "PASS"
        elif status.startswith("skipped"):
            status = "SKIP"

        results.append((unittest_group(match.group(2)), match.group(1), status))

    return results

def main():
    parser = argparse.ArgumentParser(description="Generate test reports for GitHub Pages")
    parser.add_argument("dest_dir", help="Directory to write test reports")
    parser.add_argument("--build-dir", default="build",
                        help="Directory containing test artifacts (default: build)")

    args = parser.parse_args()

    dest = os.path.abspath(args.dest_dir)
    os.makedirs(dest, exist_ok=True)

    rasdaemon = parse_tap(args.build_dir)
    ras_mc_ctl = parse_unittest()

    write_report(dest, "rasdaemon.html", "rasdaemon CMocka tests", rasdaemon)
    write_report(dest, "ras-mc-ctl.html", "ras-mc-ctl Python tests", ras_mc_ctl)

    with open(os.path.join(dest, "index.html"), "w", encoding="utf-8") as f:
        f.write(REPORT_HEADER.format(title="Test results", body=INDEX_CONTENT))

    for tool, results in (("rasdaemon", rasdaemon), ("ras-mc-ctl", ras_mc_ctl)):
        passed = sum(status == "PASS" for _, _, status in results)
        failed = sum(status in {"FAIL", "ERROR"} for _, _, status in results)

        with open(os.path.join(dest, f"{tool}-pass.svg"), "w", encoding="utf-8") as f:
            f.write(BADGE.format(value=passed, color="4c1"))

        with open(os.path.join(dest, f"{tool}-fail.svg"), "w", encoding="utf-8") as f:
            f.write(BADGE.format(value=failed, color="4c1" if failed == 0 else "e05d44"))

    shutil.copyfile(os.path.join(STATIC_DIR, "test-results.css"),
                    os.path.join(dest, "results.css"))
    shutil.copyfile(os.path.join(STATIC_DIR, "test-results.js"),
                    os.path.join(dest, "results.js"))

    return 1 if test_failed else 0


if __name__ == "__main__":
    sys.exit(main())
