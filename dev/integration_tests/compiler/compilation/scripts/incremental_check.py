#!/usr/bin/env python3

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

# Drives a full incremental-compilation cycle for a single package and asserts
# that incremental rebuilds are both *correct* (never serve stale artifacts) and
# *actually incremental* (a no-change rebuild reuses work instead of recompiling
# everything from scratch).
#
# The cycle is:
#   1. cold   : wipe the artifact dir and compile variant A from scratch.
#   2. warm   : recompile variant A WITHOUT wiping the artifact dir (no source
#               change). Output must be unchanged and the rebuild must reuse
#               work (fewer query provide-calls than the cold build).
#   3. edit   : overwrite the mutated source file with variant B and recompile
#               incrementally. Output MUST reflect variant B (a stale variant-A
#               result here is exactly the regression this test guards against).
#   4. revert : restore variant A and recompile incrementally. Output must be
#               variant A again (invalidation works in both directions).
#
# Reuse is measured via `--print-statistics`, which prints a per-query
# "Number of P-Calls" (provide calls == queries actually (re)computed rather
# than loaded from the previous query graph). If query statistics are disabled
# at compile time, the reuse assertion is skipped and only correctness is
# checked.
#
# On success the script prints "OK" to stdout and exits 0 so the integration
# test framework can apply its normal Output/ExitCode assertions. On any failure
# it prints a human-readable diagnostic to stderr and exits 1.


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Assert that incremental compilation is correct and actually reuses work."
    )
    parser.add_argument("--duckc-path", required=True)
    parser.add_argument("--vm-path", required=True, help="Path to the VM executable (dvm backend).")
    parser.add_argument("--package-dir", required=True, help="Path to the package to compile (e.g. duck_modules/incr).")
    parser.add_argument("--package-name", required=True)
    parser.add_argument("--artifact-dir", required=True, help="Top-level build artifact directory.")
    parser.add_argument("--worker-count", type=int, required=True)
    parser.add_argument("--backend", choices=["llvm", "dvm"], required=True)
    parser.add_argument("--mutate-file", required=True, help="Source file swapped between variants.")
    parser.add_argument("--variant-a", required=True, help="Canonical source for variant A.")
    parser.add_argument("--variant-b", required=True, help="Canonical source for variant B.")
    parser.add_argument("--expected-a", required=True, help="Expected program stdout for variant A (backslash escapes like \\n are decoded).")
    parser.add_argument("--expected-b", required=True, help="Expected program stdout for variant B (backslash escapes like \\n are decoded).")
    parser.add_argument("--expected-exit", type=int, default=0, help="Expected program exit code.")
    args = parser.parse_args()
    # Allow callers to pass newlines as the literal escape "\n" on the command line.
    args.expected_a = args.expected_a.encode("utf-8").decode("unicode_escape")
    args.expected_b = args.expected_b.encode("utf-8").decode("unicode_escape")
    return args


def fail(message: str) -> None:
    print(f"[incremental-check] FAILED: {message}", file=sys.stderr)
    sys.exit(1)


def run_command(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, capture_output=True, text=True)


def set_source(mutate_file: Path, variant: Path) -> None:
    shutil.copyfile(variant, mutate_file)


# Sums the per-query "Number of P-Calls" values from a --print-statistics dump.
# Returns None when query statistics are disabled at compile time (so callers
# can skip the reuse assertion rather than treating a 0 count as "no work").
def parse_provide_calls(stderr: str) -> int | None:
    if "Query statistics are disabled at compile time" in stderr:
        return None
    total = 0
    found = False
    for match in re.finditer(r"Number of P-Calls:\s*(\d+)", stderr):
        total += int(match.group(1))
        found = True
    return total if found else None


def compile_package(args: argparse.Namespace) -> tuple[subprocess.CompletedProcess[str], int | None]:
    command = [
        args.duckc_path,
        "compile_package",
        args.package_dir,
        "-w",
        str(args.worker_count),
        "-a",
        args.artifact_dir,
        "-n",
        args.package_name,
        "--print-statistics",
    ]
    if args.backend == "dvm":
        command.append("--dvm-backend")
    result = run_command(command)
    if result.returncode != 0:
        fail(
            f"compilation failed (exit {result.returncode}).\n"
            f"--- compiler stderr ---\n{result.stderr}"
        )
    return result, parse_provide_calls(result.stderr)


def run_program(args: argparse.Namespace) -> subprocess.CompletedProcess[str]:
    artifact_dir = Path(args.artifact_dir)
    if args.backend == "dvm":
        return run_command([args.vm_path, "run", str(artifact_dir / "package_dvm.dbc")])
    return run_command([str(artifact_dir / "package_llvm.exe")])


def assert_program(args: argparse.Namespace, expected_out: str, label: str) -> None:
    result = run_program(args)
    if result.stdout != expected_out:
        fail(
            f"{label}: program output mismatch.\n"
            f"expected: {expected_out!r}\n"
            f"actual:   {result.stdout!r}\n"
            f"--- program stderr ---\n{result.stderr}"
        )
    if result.returncode != args.expected_exit:
        fail(
            f"{label}: program exit code mismatch. "
            f"expected {args.expected_exit}, got {result.returncode}."
        )


def main() -> int:
    args = parse_args()
    mutate_file = Path(args.mutate_file)
    variant_a = Path(args.variant_a)
    variant_b = Path(args.variant_b)
    artifact_dir = Path(args.artifact_dir)

    try:
        # 1. cold build of variant A.
        set_source(mutate_file, variant_a)
        if artifact_dir.exists():
            shutil.rmtree(artifact_dir)
        artifact_dir.mkdir(parents=True)
        _, cold_pcalls = compile_package(args)
        assert_program(args, args.expected_a, "cold build (variant A)")

        # 2. warm no-change rebuild: must be stable and must reuse work.
        _, warm_pcalls = compile_package(args)
        assert_program(args, args.expected_a, "warm rebuild (variant A, no change)")
        if cold_pcalls is not None and warm_pcalls is not None:
            if warm_pcalls >= cold_pcalls:
                fail(
                    "no-change incremental rebuild did not reuse any work: "
                    f"cold provide-calls = {cold_pcalls}, warm provide-calls = {warm_pcalls} "
                    "(expected warm < cold). Incremental compilation is likely not loading "
                    "the previous query graph."
                )
        else:
            print(
                "[incremental-check] query statistics disabled; "
                "skipping reuse assertion, checking correctness only.",
                file=sys.stderr,
            )

        # 3. edit to variant B: output must update, not serve a stale variant-A result.
        set_source(mutate_file, variant_b)
        compile_package(args)
        assert_program(args, args.expected_b, "incremental rebuild after edit (variant B)")

        # 4. revert to variant A: invalidation must work in both directions.
        set_source(mutate_file, variant_a)
        compile_package(args)
        assert_program(args, args.expected_a, "incremental rebuild after revert (variant A)")
    finally:
        # Leave the mutated source at variant A so the working tree stays clean.
        set_source(mutate_file, variant_a)

    print("OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
