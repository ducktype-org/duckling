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


def main() -> int:
    args = parse_args()

    profiles = sorted(args.profile_dir.glob("*.profraw"))
    if not profiles:
        print(f"No profiles in {args.profile_dir}; run the tests first.", file=sys.stderr)
        return 1

    binaries = instrumented_binaries(args.build_dir)
    if not binaries:
        print(f"No instrumented binaries under {args.build_dir}.", file=sys.stderr)
        return 1

    profdata = args.build_dir / "coverage.profdata"
    subprocess.run(
        [args.llvm_profdata, "merge", "-sparse", "-o", str(profdata), *map(str, profiles)],
        check=True,
    )

    ignored = [r"^/usr/", r"^/opt/", *(ere_escape(p) for p in args.exclude)]
    objects = [str(binaries[0])]
    for binary in binaries[1:]:
        objects += ["-object", str(binary)]

    with open(args.output, "w") as out:
        subprocess.run(
            [
                args.llvm_cov,
                "export",
                "-format=lcov",
                f"-instr-profile={profdata}",
                f"-ignore-filename-regex={'|'.join(ignored)}",
                *objects,
            ],
            stdout=out,
            check=True,
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
