#!/usr/bin/env python3
"""
Race condition tester for the Duckling compiler.

Parses integration test configs (testconfig.yaml) to find all compilation test
modules, then compiles each one repeatedly with multiple workers to detect
data races via ThreadSanitizer.

Usage:
    python3 scripts/race_test.py [--iterations 100] [--workers 8] [--duckc ./build/bin/duckc]
"""

import argparse
import hashlib
import platform
import shutil
import subprocess
import sys
import time
from pathlib import Path

try:
    import yaml
except ImportError:
    print("ERROR: PyYAML required. Install with: pip install pyyaml", file=sys.stderr)
    sys.exit(1)


def parse_testconfig_tree(root_dir: Path) -> list[dict]:
    """
    Recursively parse testconfig.yaml tree starting from root_dir.
    Returns list of test entries with their module paths and compile commands.
    """
    config_file = root_dir / "testconfig.yaml"
    if not config_file.exists():
        return []

    with open(config_file) as f:
        config = yaml.safe_load(f)

    if config is None:
        return []

    results = []

    # Collect tests defined at this level
    tests = config.get("Tests", {})
    if tests:
        for test_name, test_data in tests.items():
            # Check if this test has a duck_modules directory
            duck_modules_dir = root_dir / "duck_modules" / test_name
            if duck_modules_dir.exists():
                # Extract supported backends from Cases
                backends = set()
                cases = (test_data or {}).get("Cases", {})
                if cases:
                    for case_data in cases.values():
                        if case_data and "backend" in case_data:
                            backends.add(case_data["backend"])

                results.append({
                    "name": test_name,
                    "duck_modules_dir": duck_modules_dir,
                    "test_dir": root_dir,
                    "config": config,
                    "test_data": test_data,
                    "backends": backends,
                })

    # Recurse into subdirectories
    sub_dirs = config.get("SubDirs", [])
    for sub in sub_dirs:
        sub_path = root_dir / sub
        if sub_path.exists():
            results.extend(parse_testconfig_tree(sub_path))

    return results


def md5_file(path: Path) -> str:
    """Compute MD5 hash of a file."""
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(8192), b""):
            h.update(chunk)
    return h.hexdigest()


def compare_artifact_hashes(multi_dir: Path, single_dir: Path) -> list[str]:
    """
    Compare .o files between multi-threaded and single-threaded artifact dirs.
    Returns list of mismatch descriptions. Empty list means all match.

    Artifact structure: <art_dir>/query/query<N>/<hash>.o
    """
    mismatches = []

    if not multi_dir.exists():
        mismatches.append(f"Multi-threaded artifact dir missing: {multi_dir}")
        return mismatches
    if not single_dir.exists():
        mismatches.append(f"Single-threaded artifact dir missing: {single_dir}")
        return mismatches

    # Collect all .o files relative to artifact root
    multi_o_files = {
        p.relative_to(multi_dir): p
        for p in multi_dir.rglob("*.o")
    }
    single_o_files = {
        p.relative_to(single_dir): p
        for p in single_dir.rglob("*.o")
    }

    # Check for files only in one side
    only_multi = set(multi_o_files.keys()) - set(single_o_files.keys())
    only_single = set(single_o_files.keys()) - set(multi_o_files.keys())

    for f in sorted(only_multi):
        mismatches.append(f"Only in multi-threaded: {f}")
    for f in sorted(only_single):
        mismatches.append(f"Only in single-threaded: {f}")

    # Compare hashes of matching files
    common = set(multi_o_files.keys()) & set(single_o_files.keys())
    for rel_path in sorted(common):
        hash_multi = md5_file(multi_o_files[rel_path])
        hash_single = md5_file(single_o_files[rel_path])
        if hash_multi != hash_single:
            mismatches.append(
                f"MISMATCH {rel_path}: multi={hash_multi} single={hash_single}"
            )

    return mismatches


def main():
    parser = argparse.ArgumentParser(
        description="Race condition tester — compiles integration test modules repeatedly to detect TSan races"
    )
    parser.add_argument(
        "--iterations", "-i",
        type=int,
        default=30,
        help="Number of compilation iterations per package (default: 100)",
    )
    parser.add_argument(
        "--workers", "-w",
        type=int,
        default=30,
        help="Number of worker threads (default: 8)",
    )
    parser.add_argument(
        "--duckc",
        type=str,
        default=None,
        help="Path to duckc binary (default: ./build/bin/duckc)",
    )
    parser.add_argument(
        "--output", "-o",
        type=str,
        default=None,
        help="Output log file (default: race_test_<timestamp>.log)",
    )
    parser.add_argument(
        "--filter", "-f",
        type=str,
        default=None,
        help="Only run tests whose name contains this string",
    )
    parser.add_argument(
        "--list", "-l",
        action="store_true",
        help="List all test modules and exit (don't run)",
    )
    parser.add_argument(
        "--dvm-backend",
        action="store_true",
        help="Compile to DVM bytecode instead of LLVM (passes --dvm-backend to duckc)",
    )
    parser.add_argument(
        "--no-determinism-check",
        action="store_true",
        help="Disable determinism check (comparing .o files between single and multi-threaded runs)",
    )
    args = parser.parse_args()

    # Determine paths
    dev_dir = Path(__file__).resolve().parent.parent
    duckc = Path(args.duckc) if args.duckc else dev_dir / "build" / "bin" / "duckc"
    itest_dir = dev_dir / "integration_tests"
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    output_file = Path(args.output) if args.output else dev_dir / f"race_test_{timestamp}.log"

    if not duckc.exists() and not args.list:
        print(f"ERROR: duckc not found at {duckc}", file=sys.stderr)
        sys.exit(1)

    if not itest_dir.exists():
        print(f"ERROR: integration tests not found at {itest_dir}", file=sys.stderr)
        sys.exit(1)

    # Parse all test configs
    all_tests = parse_testconfig_tree(itest_dir)

    # Filter to only compilation tests (those with duck_modules dirs)
    compile_tests = [t for t in all_tests if t["duck_modules_dir"].exists()]

    # Filter by backend: only run tests that support the selected backend
    target_backend = "dvm" if args.dvm_backend else "llvm"
    compile_tests = [
        t for t in compile_tests
        if target_backend in t["backends"] or not t["backends"]
    ]

    # Apply name filter if provided
    if args.filter:
        compile_tests = [t for t in compile_tests if args.filter in t["name"]]

    # Sort for deterministic order
    compile_tests.sort(key=lambda t: t["name"])

    print(f"Found {len(compile_tests)} compilation test modules:")
    for i, test in enumerate(compile_tests, 1):
        rel = test["duck_modules_dir"].relative_to(dev_dir)
        print(f"  {i:3d}. {test['name']:40s} ({rel})")

    if args.list:
        return

    backend_str = "DVM" if args.dvm_backend else "LLVM"
    det_str = "OFF" if args.no_determinism_check else "ON"
    print(f"\nSettings: {args.iterations} iterations x {args.workers} workers (backend: {backend_str}, determinism check: {det_str})")
    print(f"Output:   {output_file}")
    print(f"duckc:    {duckc}")
    print()

    # Run tests
    total_failures = 0
    total_races = 0
    total_determinism_failures = 0
    total_race_details: dict[str, list[str]] = {}
    total_determinism_details: dict[str, list[str]] = {}

    with open(output_file, "w") as log:
        log.write(f"Race Condition Test Report\n")
        log.write(f"{'=' * 60}\n")
        log.write(f"Date:       {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
        log.write(f"duckc:      {duckc}\n")
        log.write(f"Iterations: {args.iterations}\n")
        log.write(f"Workers:    {args.workers}\n")
        log.write(f"Backend:    {backend_str}\n")
        log.write(f"Modules:    {len(compile_tests)}\n")
        log.write(f"{'=' * 60}\n\n")

        for test_idx, test in enumerate(compile_tests, 1):
            test_name = test["name"]
            test_dir = test["test_dir"]

            header = f"[{test_idx}/{len(compile_tests)}] {test_name}"
            print(f"\n{'─' * 60}")
            print(f"{header}")
            print(f"  dir: {test_dir.relative_to(dev_dir)}")
            print(f"{'─' * 60}")

            log.write(f"\n{'─' * 60}\n")
            log.write(f"{header}\n")
            log.write(f"  dir: {test_dir}\n")
            log.write(f"{'─' * 60}\n")

            pkg_failures = 0
            pkg_races = 0
            pkg_determinism_failures = 0
            start_time = time.time()

            for run in range(1, args.iterations + 1):
                # Use the same -n name for both compilations so .o files match
                unique_name = f"race_{test_name}_{run}"
                art_multi = f"race_art_{test_name}_{run}_multi"
                art_single = f"race_art_{test_name}_{run}_single"

                # Use setarch to disable ASLR — required for TSan on
                # modern kernels (6.x+) where PIE binaries can be mapped
                # at addresses that collide with TSan's shadow memory.
                base_cmd = [
                    "setarch", platform.machine(), "-R",
                    str(duckc),
                    "compile_package",
                    f"duck_modules/{test_name}",
                    "-n", unique_name,
                    "--no-incremental",
                ]

                if args.dvm_backend:
                    base_cmd.append("--dvm-backend")

                cmd_multi = base_cmd + ["-w", str(args.workers), "-a", art_multi]
                cmd_single = base_cmd + ["-w", "1", "-a", art_single]

                try:
                    # --- Multi-threaded compilation ---
                    result = subprocess.run(
                        cmd_multi,
                        capture_output=True,
                        text=True,
                        timeout=120,
                        cwd=str(test_dir),
                    )

                    stderr = result.stderr
                    has_race = "ThreadSanitizer: data race" in stderr
                    has_tsan_warning = "WARNING: ThreadSanitizer" in stderr
                    failed = result.returncode != 0

                    if has_race or has_tsan_warning:
                        pkg_races += 1
                        total_races += 1
                        status = "RACE"
                        for line in stderr.splitlines():
                            if "SUMMARY: ThreadSanitizer" in line:
                                if test_name not in total_race_details:
                                    total_race_details[test_name] = []
                                total_race_details[test_name].append(line.strip())
                    elif failed:
                        pkg_failures += 1
                        total_failures += 1
                        status = f"FAIL(rc={result.returncode})"
                    else:
                        status = "OK"

                    # Log failures and races in detail
                    if status != "OK":
                        log.write(f"\n  Run {run}: {status}\n")
                        log.write(f"  Command: {' '.join(cmd_multi)}\n")
                        log.write(f"  CWD: {test_dir}\n")
                        if stderr.strip():
                            log.write(f"  --- stderr ---\n")
                            log.write(f"{stderr}\n")
                            log.write(f"  --- end stderr ---\n")

                    # --- Determinism check: single-threaded compilation + compare ---
                    if not args.no_determinism_check and status == "OK":
                        result_single = subprocess.run(
                            cmd_single,
                            capture_output=True,
                            text=True,
                            timeout=120,
                            cwd=str(test_dir),
                        )

                        if result_single.returncode != 0:
                            pkg_failures += 1
                            total_failures += 1
                            status = f"FAIL_SINGLE(rc={result_single.returncode})"
                            log.write(f"\n  Run {run}: {status}\n")
                            log.write(f"  Command: {' '.join(cmd_single)}\n")
                            if result_single.stderr.strip():
                                log.write(f"  --- stderr ---\n")
                                log.write(f"{result_single.stderr}\n")
                                log.write(f"  --- end stderr ---\n")
                        else:
                            # Compare .o files between multi and single
                            mismatches = compare_artifact_hashes(
                                test_dir / art_multi,
                                test_dir / art_single,
                            )
                            if mismatches:
                                pkg_determinism_failures += 1
                                total_determinism_failures += 1
                                status = "NONDETERMINISTIC"
                                if test_name not in total_determinism_details:
                                    total_determinism_details[test_name] = []
                                total_determinism_details[test_name].extend(mismatches)
                                log.write(f"\n  Run {run}: {status}\n")
                                log.write(f"  Mismatched .o files:\n")
                                for m in mismatches:
                                    log.write(f"    {m}\n")

                    # Progress indicator
                    if run % 10 == 0 or status != "OK":
                        print(f"  run {run:3d}/{args.iterations}: {status}")

                except subprocess.TimeoutExpired:
                    pkg_failures += 1
                    total_failures += 1
                    print(f"  run {run:3d}/{args.iterations}: TIMEOUT")
                    log.write(f"\n  Run {run}: TIMEOUT (120s)\n")
                    log.write(f"  Command: {' '.join(cmd_multi)}\n")
                    log.write(f"  CWD: {test_dir}\n")
                finally:
                    # Clean up both artifact directories
                    for art_name in (art_multi, art_single):
                        art_dir = test_dir / art_name
                        if art_dir.exists():
                            shutil.rmtree(art_dir, ignore_errors=True)

            elapsed = time.time() - start_time
            summary = (
                f"  Result: {args.iterations} runs in {elapsed:.1f}s | "
                f"races: {pkg_races} | failures: {pkg_failures} | nondeterministic: {pkg_determinism_failures}"
            )
            print(summary)
            log.write(f"\n{summary}\n")

        # Write final report to log
        log.write(f"\n\n{'=' * 60}\n")
        log.write(f"FINAL REPORT\n")
        log.write(f"{'=' * 60}\n")
        log.write(f"  Modules tested:         {len(compile_tests)}\n")
        log.write(f"  Total races:            {total_races}\n")
        log.write(f"  Total failures:         {total_failures}\n")
        log.write(f"  Total nondeterministic: {total_determinism_failures}\n\n")

        if total_race_details:
            log.write(f"Race locations (unique SUMMARY lines):\n")
            for module, summaries in sorted(total_race_details.items()):
                unique_summaries = sorted(set(summaries))
                log.write(f"\n  [{module}] ({len(summaries)} occurrences):\n")
                for s in unique_summaries:
                    log.write(f"    {s}\n")

        if total_determinism_details:
            log.write(f"\nNondeterministic .o files:\n")
            for module, details in sorted(total_determinism_details.items()):
                unique_details = sorted(set(details))
                log.write(f"\n  [{module}] ({len(details)} occurrences):\n")
                for d in unique_details:
                    log.write(f"    {d}\n")

    # Print final report to terminal
    print(f"\n{'=' * 60}")
    print(f"FINAL REPORT")
    print(f"{'=' * 60}")
    print(f"  Modules tested:         {len(compile_tests)}")
    print(f"  Total races:            {total_races}")
    print(f"  Total failures:         {total_failures}")
    print(f"  Total nondeterministic: {total_determinism_failures}")
    print(f"  Log file:               {output_file}")

    if total_race_details:
        print(f"\n  Race locations:")
        for module, summaries in sorted(total_race_details.items()):
            unique_summaries = sorted(set(summaries))
            print(f"    [{module}] ({len(summaries)} occurrences):")
            for s in unique_summaries:
                print(f"      {s}")

    if total_determinism_details:
        print(f"\n  Nondeterministic .o files:")
        for module, details in sorted(total_determinism_details.items()):
            unique_details = sorted(set(details))
            print(f"    [{module}] ({len(details)} occurrences):")
            for d in unique_details:
                print(f"      {d}")

    has_problems = total_races > 0 or total_failures > 0 or total_determinism_failures > 0
    if has_problems:
        print(f"\n  PROBLEMS DETECTED -- check {output_file} for details")
        sys.exit(1)
    else:
        print(f"\n  All clean -- no races or nondeterminism detected")
        sys.exit(0)


if __name__ == "__main__":
    main()
