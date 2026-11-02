#!/usr/bin/env python3
# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

"""
Run a command and assert its output/exit code, matching stdout and/or stderr
against regexes instead of comparing them literally.

Every `--*-regex` is `re.search`-ed (matches anywhere, need not be anchored)
and may be repeated to require several patterns. On any failed assertion a
diagnostic naming what differed (and the captured output) is written to stderr
and the script exits 1.

Usage:
    expect_output.py [--out-regex PATTERN ...] [--err-regex PATTERN ...]
                     [--expect-exit N] -- CMD [ARG ...]
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Run a command and assert its stdout/stderr against regexes and "
            "its exit code, tolerating volatile output."
        )
    )
    parser.add_argument(
        "--out-regex",
        action="append",
        default=[],
        metavar="PATTERN",
        dest="out_regexes",
        help="Regex that must be found in stdout (may be repeated).",
    )
    parser.add_argument(
        "--err-regex",
        action="append",
        default=[],
        metavar="PATTERN",
        dest="err_regexes",
        help="Regex that must be found in stderr (may be repeated).",
    )
    parser.add_argument(
        "--expect-exit",
        type=int,
        default=None,
        metavar="N",
        help="Exit code the command is expected to return.",
    )
    parser.add_argument(
        "command",
        nargs=argparse.REMAINDER,
        help="The command to run, after a `--` separator.",
    )
    return parser.parse_args()


def head(text: str, n: int = 50) -> str:
    lines = text.splitlines(keepends=True)
    result = "".join(lines[:n])
    if len(lines) > n:
        result += f"... ({len(lines) - n} more line(s) omitted)\n"
    return result


def main() -> int:
    args = parse_args()

    command = args.command
    # argparse.REMAINDER keeps the leading `--`, drop it.
    if command and command[0] == "--":
        command = command[1:]
    if not command:
        print("[expect-output] No command given after `--`.", file=sys.stderr)
        return 1

    # shell=False: the command and its args are already split by the caller,
    # so no shell quoting surprises with the regex patterns.
    result = subprocess.run(command, capture_output=True, text=True)

    problems: list[str] = []

    for pattern in args.out_regexes:
        if re.search(pattern, result.stdout) is None:
            problems.append(f"  stdout did not match regex: {pattern!r}")

    for pattern in args.err_regexes:
        if re.search(pattern, result.stderr) is None:
            problems.append(f"  stderr did not match regex: {pattern!r}")

    if args.expect_exit is not None and result.returncode != args.expect_exit:
        problems.append(
            f"  exit code was {result.returncode}, expected {args.expect_exit}"
        )

    if problems:
        report = ["[expect-output] Command output did not match expectations.\n"]
        report.append(f"  command: {' '.join(command)}\n")
        report.extend(p + "\n" for p in problems)
        report.append(f"  actual exit code: {result.returncode}\n")
        report.append("  actual stdout (first 50 lines):\n")
        report.append(head(result.stdout))
        report.append("  actual stderr (first 50 lines):\n")
        report.append(head(result.stderr))
        sys.stderr.write("".join(report))
        sys.stderr.flush()
        return 1

    # Success: forward only the (stable) stdout so the framework's `Output`
    # assertion still applies; swallow the volatile stderr.
    sys.stdout.write(result.stdout)
    sys.stdout.flush()
    return 0


if __name__ == "__main__":
    sys.exit(main())
