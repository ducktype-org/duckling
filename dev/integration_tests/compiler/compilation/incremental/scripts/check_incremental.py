#!/usr/bin/env python3
"""Run duckc against a package with a warm artifacts directory and validate incrementality.

The compiler prints one "[id/total]  Compiling <module> (<backend>): <status>!" line per
(module, backend) pair on stderr. This script runs a single compilation (the caller is expected
to have populated the artifacts directory in a previous run), then asserts:

- every module listed in --expect-recompiled is "Recompiling", everything else is
  "Cached, loading artifact from disk";
- task numbering is sane in every batch (batches are delimited by the id counter resetting):
  no "?" placeholders, ids form exactly the set 1..total, and total equals the highest id.

Prints "INCREMENTAL OK" on success so the DIT case can match stdout exactly.
"""

import argparse
import re
import subprocess
import sys

LOG_RE = re.compile(
    r"\[(?P<id>\d+|\?)/(?P<total>\d+|\?)\]\s+Compiling (?P<module>\S+) "
    r"\((?P<backend>\w+)\): (?P<status>[^!]+)!"
)

CACHED = "Cached, loading artifact from disk"
RECOMPILING = "Recompiling"


def fail(message: str, log: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    print("--- duckc log ---", file=sys.stderr)
    print(log, file=sys.stderr)
    sys.exit(1)


def check_numbering(entries: list[dict], log: str) -> None:
    # With multiple workers lines interleave, so batches cannot be detected by id order.
    # A new batch starts when an id is reused by a different module (ids are unique per
    # module within a batch; the same module may appear once per backend).
    batches: list[list[dict]] = []
    id_to_module: dict[int, str] = {}
    for entry in entries:
        if entry["id"] == "?" or entry["total"] == "?":
            fail(f"numbering contains '?': {entry}", log)
        entry_id = int(entry["id"])
        if not batches or id_to_module.get(entry_id, entry["module"]) != entry["module"]:
            batches.append([])
            id_to_module = {}
        id_to_module[entry_id] = entry["module"]
        batches[-1].append(entry)

    for batch in batches:
        totals = {int(e["total"]) for e in batch}
        if len(totals) != 1:
            fail(f"inconsistent totals within one batch: {sorted(totals)}", log)
        total = totals.pop()
        ids = {int(e["id"]) for e in batch}
        if ids != set(range(1, total + 1)):
            fail(f"batch ids {sorted(ids)} do not cover 1..{total}", log)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--duckc", required=True)
    parser.add_argument("--package", required=True)
    parser.add_argument("--artifacts", required=True)
    parser.add_argument("--workers", default="1")
    parser.add_argument("--dvm", action="store_true")
    parser.add_argument(
        "--expect-recompiled",
        default="",
        help="comma-separated module names expected to be 'Recompiling'; empty means none",
    )
    args = parser.parse_args()

    command = [
        args.duckc,
        "compile_package",
        args.package,
        "-w",
        args.workers,
        "-a",
        args.artifacts,
        "-n",
        "pkg",
    ]
    if args.dvm:
        command.append("--dvm-backend")

    result = subprocess.run(command, capture_output=True, text=True)
    log = result.stderr + result.stdout
    if result.returncode != 0:
        fail(f"duckc exited with {result.returncode}", log)

    entries = [m.groupdict() for m in LOG_RE.finditer(log)]
    if not entries:
        fail("no 'Compiling' lines found in duckc output", log)

    expected_recompiled = {name for name in args.expect_recompiled.split(",") if name}
    recompiled = {e["module"] for e in entries if e["status"] == RECOMPILING}
    cached = {e["module"] for e in entries if e["status"] == CACHED}
    other = {e["module"] for e in entries if e["status"] not in (RECOMPILING, CACHED)}

    if other:
        fail(f"unexpected module status for: {sorted(other)}", log)
    if recompiled != expected_recompiled:
        fail(
            f"recompiled set {sorted(recompiled)} != expected {sorted(expected_recompiled)}",
            log,
        )
    if cached & expected_recompiled:
        fail(f"modules both cached and expected recompiled: {sorted(cached & recompiled)}", log)

    check_numbering(entries, log)

    print("INCREMENTAL OK")


if __name__ == "__main__":
    main()
