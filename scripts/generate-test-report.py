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
    <table id="results"><thead><tr><th>Test</th><th>Result</th></tr></thead><tbody>
""")

TABLE_ROW = dedent("""\
    <tr><td>{name}</td><td class="{status}">{status}</td></tr>
""")

TABLE_FOOTER = dedent("""\
    </tbody></table>
""")


def table(title, results):
    rows = ""

    for name, status in results:
        name = name.removeprefix("test_").replace("_", " ")
        name = name.split("(")[0].strip()

        rows += TABLE_ROW.format(name=name, status=status) + "\n"

    return TABLE_HEADER.format(title=html.escape(title)) + rows + TABLE_FOOTER


def write_report(output, filename, title, results):
    with open(os.path.join(output, filename), "w", encoding="utf-8") as f:
        f.write(REPORT_HEADER.format(title=html.escape(title), body=table(title, results)))

def run_cmd(cmd):
    result = subprocess.run(cmd, capture_output=True, text=True, check=True)

    output = result.stdout + result.stderr

    print(output)

    return output

def parse_tap(build_dir):
    results = []
    pattern = re.compile(r"^(not )?ok \d+ - (.*?)(?:\s+#.*)?$")

    out = run_cmd([f"{build_dir}/unittest", "-o", "tap"])

    for line in out.splitlines():
        match = pattern.match(line)
        if match:
            results.append((match.group(2), "FAIL" if match.group(1) else "PASS"))

    return results

def parse_unittest():
    results = []
    pattern = re.compile(r"^(test_.*) \.\.\. (ok|FAIL|ERROR|skipped.*)$")

    out = run_cmd(["tests/run.py", "-vvv"])

    for line in out.splitlines():
        match = pattern.match(line)
        if not match:
            continue

        status = match.group(2)
        if status == "ok":
            status = "PASS"
        elif status.startswith("skipped"):
            status = "SKIP"

        results.append((match.group(1), status))

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

    with open(os.path.join(dest, "index.html"), "w") as f:
        f.write(REPORT_HEADER.format(title="Test results", body=INDEX_CONTENT))

    for tool, results in (("rasdaemon", rasdaemon), ("ras-mc-ctl", ras_mc_ctl)):
        passed = sum(status == "PASS" for _, status in results)
        failed = sum(status in {"FAIL", "ERROR"} for _, status in results)

        with open(os.path.join(dest, f"{tool}-pass.svg"), "w", encoding="utf-8") as f:
            f.write(BADGE.format(value=passed, color="4c1"))

        with open(os.path.join(dest, f"{tool}-fail.svg"), "w", encoding="utf-8") as f:
            f.write(BADGE.format(value=failed, color="4c1" if failed == 0 else "e05d44"))

    shutil.copyfile(os.path.join(STATIC_DIR, "test-results.css"),
                    os.path.join(dest, "results.css"))
    shutil.copyfile(os.path.join(STATIC_DIR, "test-results.js"),
                    os.path.join(dest, "results.js"))


if __name__ == "__main__":
    main()
