# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

"""
Compile a package scenario twice on the same artifacts directory and run the result.

The scenario directory has two subdirectories, `step1` and `step2`, each with a full
`manifest.json` (and the sources it needs). `step1` is copied into the work directory, compiled
and its `app_exe.dbc` is run - its exit code must be `--step1-exit`, so the second step really
starts from a successful build. Then `step2` is copied over it (sources missing from `step2` are
kept) and compiled incrementally, reusing the artifacts of `step1`.

The exit code of the script is the exit code of the `step2` program, or of `duckc` when the
`step2` compilation fails (its output is forwarded). A problem with `step1` exits with 100.

Usage:
    run_steps.py --duckc PATH --vm PATH --scenario DIR --work DIR
                 --step1-flags FLAGS --step2-flags FLAGS --step1-exit N
"""

from __future__ import annotations

import argparse
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

STEP1_FAILED = 100


def compile_step(duckc: str, work: Path, flags: str) -> subprocess.CompletedProcess:
    return subprocess.run(
        [duckc, "compile_packages", "manifest.json", *shlex.split(flags), "-a", "build"],
        cwd=work,
        capture_output=True,
        text=True,
    )


def run_app(vm: str, work: Path) -> int:
    return subprocess.run([vm, "run", "build/app_exe.dbc"], cwd=work).returncode


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    parser.add_argument("--duckc", required=True)
    parser.add_argument("--vm", required=True)
    parser.add_argument("--scenario", required=True, type=Path)
    parser.add_argument("--work", required=True, type=Path)
    parser.add_argument("--step1-flags", default="")
    parser.add_argument("--step2-flags", default="")
    parser.add_argument("--step1-exit", required=True, type=int)
    args = parser.parse_args()

    shutil.rmtree(args.work, ignore_errors=True)
    shutil.copytree(args.scenario / "step1", args.work)
    # duckc requires the artifacts directory to exist when it is under the temporary directory.
    (args.work / "build").mkdir()

    step1 = compile_step(args.duckc, args.work, args.step1_flags)
    if step1.returncode != 0:
        sys.stderr.write(f"step1 compilation failed:\n{step1.stdout}{step1.stderr}")
        return STEP1_FAILED
    if (step1_exit := run_app(args.vm, args.work)) != args.step1_exit:
        sys.stderr.write(f"step1 program exited with {step1_exit}, expected {args.step1_exit}\n")
        return STEP1_FAILED

    shutil.copytree(args.scenario / "step2", args.work, dirs_exist_ok=True)

    step2 = compile_step(args.duckc, args.work, args.step2_flags)
    if step2.returncode != 0:
        sys.stdout.write(step2.stdout)
        sys.stderr.write(step2.stderr)
        return step2.returncode
    return run_app(args.vm, args.work)


if __name__ == "__main__":
    sys.exit(main())
