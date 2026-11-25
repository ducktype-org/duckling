#!/usr/bin/env python3
# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

"""Turns clang source-based coverage profiles into an lcov tracefile."""

import argparse
import os
import re
import subprocess
import sys
import time
from pathlib import Path

ELF_MAGIC = b"\x7fELF"
COVERAGE_SECTION = b"__llvm_covfun"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--llvm-profdata", required=True)
    parser.add_argument("--llvm-cov", required=True)
    parser.add_argument("--profile-dir", required=True, type=Path)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--exclude", nargs="*", default=[])
    parser.add_argument("-o", "--output", required=True, type=Path)
    return parser.parse_args()


def instrumented_binaries(build_dir: Path) -> list[Path]:
    """Linked executables and shared libraries that carry a coverage mapping."""
    found = []
    for root, dirs, files in os.walk(build_dir):
        dirs[:] = [d for d in dirs if d != "CMakeFiles"]
        for name in files:
            path = Path(root) / name
            if path.suffix in (".o", ".a") or path.is_symlink():
                continue
            if not (os.access(path, os.X_OK) or ".so" in path.suffixes):
                continue
            with open(path, "rb") as file:
                if file.read(4) != ELF_MAGIC:
                    continue
                if COVERAGE_SECTION in file.read():
                    found.append(path)
    return sorted(found)


def ere_escape(text: str) -> str:
    """llvm-cov takes POSIX extended regexes, where Python's re.escape output is not valid."""
    return re.sub(r"([.^$|()\[\]*+?{}\\])", r"\\\1", text)


def drop_lines_past_eof(tracefile: str) -> str:
    """llvm-cov maps X-macro .def.hpp files one line past their end, which genhtml rejects."""
    out = []
    line_count = None
    found = hit = 0
    for record in tracefile.splitlines():
        if record.startswith("SF:"):
            with open(record[3:], "rb") as source:
                line_count = len(source.read().splitlines())
            found = hit = 0
        elif record.startswith(("DA:", "BRDA:")):
            number = int(record.split(":", 1)[1].split(",", 1)[0])
            if number > line_count:
                continue
            if record.startswith("DA:"):
                found += 1
                hit += int(record.split(",")[1]) > 0
        elif record.startswith("LF:"):
            record = f"LF:{found}"
        elif record.startswith("LH:"):
            record = f"LH:{hit}"
        out.append(record)
    return "\n".join(out) + "\n"


def main() -> int:
    args = parse_args()

    profiles = sorted(args.profile_dir.glob("*.profraw"))
    if not profiles:
        print(f"No profiles in {args.profile_dir}; run the tests first.", file=sys.stderr)
        return 1

    started = time.monotonic()
    binaries = instrumented_binaries(args.build_dir)
    print(f"Found {len(binaries)} instrumented binaries in {time.monotonic() - started:.1f}s")
    if not binaries:
        print(f"No instrumented binaries under {args.build_dir}.", file=sys.stderr)
        return 1

    profdata = args.build_dir / "coverage.profdata"
    started = time.monotonic()
    subprocess.run(
        [args.llvm_profdata, "merge", "-sparse", "-o", str(profdata), *map(str, profiles)],
        check=True,
    )
    print(f"Merged {len(profiles)} profiles in {time.monotonic() - started:.1f}s")

    ignored = [r"^/usr/", r"^/opt/", *(ere_escape(p) for p in args.exclude)]
    objects = [str(binaries[0])]
    for binary in binaries[1:]:
        objects += ["-object", str(binary)]

    started = time.monotonic()
    exported = subprocess.run(
        [
            args.llvm_cov,
            "export",
            "-format=lcov",
            f"-instr-profile={profdata}",
            f"-ignore-filename-regex={'|'.join(ignored)}",
            *objects,
        ],
        stdout=subprocess.PIPE,
        text=True,
        check=True,
    ).stdout
    print(f"Exported coverage in {time.monotonic() - started:.1f}s")
    args.output.write_text(drop_lines_past_eof(exported))
    return 0


if __name__ == "__main__":
    sys.exit(main())
