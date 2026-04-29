#!/usr/bin/env python3

from __future__ import annotations

import argparse
import subprocess
import sys


# Runs an arbitrary number of shell commands and verifies that they all produce
# identical stdout, stderr, and exit code.
#
# On success (all outputs match) the common stdout is written to stdout, the
# common stderr is written to stderr, and the process exits with the shared
# exit code so that the integration-test framework can apply its normal
# Output/Err/ExitCode assertions.
#
# On mismatch the script prints a human-readable diagnostic to stderr that
# shows which fields differed and, for each command, the first 50 lines of its
# stdout and stderr, then exits with code 1.
#
# Usage:
#   compare_runs.py --cmd LABEL "shell command" [--cmd LABEL "shell command" ...]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Run shell commands and assert that they produce identical "
            "stdout, stderr, and exit code."
        )
    )
    parser.add_argument(
        "--cmd",
        nargs=2,
        metavar=("LABEL", "COMMAND"),
        action="append",
        dest="commands",
        required=True,
        help="A label and a shell command to run (may be repeated).",
    )
    return parser.parse_args()


# Runs a single shell command and returns (stdout, stderr, exit_code).
def run_command(shell_cmd: str) -> tuple[str, str, int]:
    result = subprocess.run(shell_cmd, shell=True, capture_output=True, text=True)
    return result.stdout, result.stderr, result.returncode


# Formats the first `n` lines of text for diagnostic output.
def head(text: str, n: int = 50) -> str:
    lines = text.splitlines(keepends=True)
    truncated = lines[:n]
    result = "".join(truncated)
    if len(lines) > n:
        result += f"... ({len(lines) - n} more line(s) omitted)\n"
    return result


def main() -> int:
    args = parse_args()

    if len(args.commands) < 2:
        print(
            "[compare-runs] At least two --cmd entries are required.",
            file=sys.stderr,
        )
        return 1

    labels: list[str] = []
    results: list[tuple[str, str, int]] = []

    for label, shell_cmd in args.commands:
        labels.append(label)
        stdout, stderr, code = run_command(shell_cmd)
        results.append((stdout, stderr, code))

    stdouts = [r[0] for r in results]
    stderrs = [r[1] for r in results]
    exit_codes = [r[2] for r in results]

    stdout_match = all(s == stdouts[0] for s in stdouts)
    stderr_match = all(s == stderrs[0] for s in stderrs)
    code_match = all(c == exit_codes[0] for c in exit_codes)

    if stdout_match and stderr_match and code_match:
        sys.stdout.write(stdouts[0])
        sys.stdout.flush()
        sys.stderr.write(stderrs[0])
        sys.stderr.flush()
        return exit_codes[0]

    # At least one field differs — build a diagnostic report.
    lines: list[str] = ["[compare-runs] Commands produced different results.\n"]

    if not code_match:
        code_summary = ", ".join(f"{label}: {code}" for label, code in zip(labels, exit_codes))
        lines.append(f"  Exit codes differ — {code_summary}\n")

    if not stdout_match:
        lines.append("  Stdouts differ:\n")
        for label, stdout in zip(labels, stdouts):
            lines.append(f"    [{label}] stdout (first 50 lines):\n")
            for output_line in head(stdout).splitlines(keepends=True):
                lines.append(f"      {output_line}")

    if not stderr_match:
        lines.append("  Stderrs differ:\n")
        for label, stderr in zip(labels, stderrs):
            lines.append(f"    [{label}] stderr (first 50 lines):\n")
            for output_line in head(stderr).splitlines(keepends=True):
                lines.append(f"      {output_line}")

    sys.stderr.write("".join(lines))
    sys.stderr.flush()
    return 1


if __name__ == "__main__":
    sys.exit(main())
