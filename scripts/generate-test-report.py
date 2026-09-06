#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Create GitHub Pages test reports and numeric status badges."""

import html
import pathlib
import re
import sys


def parse_tap(path):
    results = []
    pattern = re.compile(r"^(not )?ok \d+ - (.*?)(?:\s+#.*)?$")
    for line in pathlib.Path(path).read_text().splitlines():
        match = pattern.match(line)
        if match:
            results.append((match.group(2), "FAIL" if match.group(1) else "PASS"))
    return results


def parse_unittest(path):
    results = []
    pattern = re.compile(r"^(test_.*) \.\.\. (ok|FAIL|ERROR|skipped.*)$")
    for line in pathlib.Path(path).read_text().splitlines():
        match = pattern.match(line)
        if match:
            status = match.group(2)
            results.append((match.group(1), "PASS" if status == "ok" else status.upper()))
    return results


def badge(value, color):
    return (
        '<svg xmlns="http://www.w3.org/2000/svg" width="28" height="20">'
        f'<rect width="28" height="20" fill="#{color}"/>'
        f'<text x="7" y="14" fill="#fff" font-family="Verdana" '
        f'font-size="11">{value}</text></svg>'
    )


def table(title, results):
    rows = "\n".join(
        f"<tr><td>{html.escape(name)}</td><td class=\"{status.lower()}\">"
        f"{html.escape(status)}</td></tr>"
        for name, status in results
    )
    return f"<h2>{html.escape(title)}</h2><table><tr><th>Test</th><th>Result</th></tr>{rows}</table>"


def page(title, body):
    return f"""<!doctype html>
<html><head><meta charset=\"utf-8\"><title>{html.escape(title)}</title>
<style>body{{font-family:sans-serif;margin:2rem}}table{{border-collapse:collapse;width:100%}}th,td{{border:1px solid #ddd;padding:.4rem;text-align:left}}.pass{{color:#16803c}}.fail,.error{{color:#c33}}.skipped{{color:#856404}}</style>
</head><body><h1>{html.escape(title)}</h1><p><a href=\"../index.html\">Documentation</a> | <a href=\"index.html\">All test results</a></p>{body}</body></html>"""


def write_report(output, filename, title, results):
    output.joinpath(filename).write_text(page(title, table(title, results)))


def main():
    output = pathlib.Path(sys.argv[3])
    output.mkdir(parents=True, exist_ok=True)
    rasdaemon = parse_tap(sys.argv[1])
    ras_mc_ctl = parse_unittest(sys.argv[2])

    write_report(output, "rasdaemon.html", "rasdaemon CMocka tests", rasdaemon)
    write_report(output, "ras-mc-ctl.html", "ras-mc-ctl Python tests", ras_mc_ctl)
    index = (
        '<p><a href="rasdaemon.html">rasdaemon CMocka tests</a></p>'
        '<p><a href="ras-mc-ctl.html">ras-mc-ctl Python tests</a></p>'
        + table("rasdaemon CMocka tests", rasdaemon)
        + table("ras-mc-ctl Python tests", ras_mc_ctl)
    )
    output.joinpath("index.html").write_text(page("Test results", index))

    for tool, results in (("rasdaemon", rasdaemon), ("ras-mc-ctl", ras_mc_ctl)):
        passed = sum(status == "PASS" for _, status in results)
        failed = sum(status in {"FAIL", "ERROR"} for _, status in results)
        output.joinpath(f"{tool}-pass.svg").write_text(badge(passed, "4c1"))
        output.joinpath(f"{tool}-fail.svg").write_text(
            badge(failed, "4c1" if failed == 0 else "e05d44")
        )


if __name__ == "__main__":
    main()
