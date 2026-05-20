#!/usr/bin/env python3
"""
Generate and run compile benchmarks for two script shapes:
`unique`, which calls every function once, and `repeated`, which calls one
function many times. For fair comparison, the repeated scenario has the same function definitions as the unique scenario, but just calls one of them repeatedly.
"""

import argparse
import csv
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path


DEFAULT_STATEMENTS = 4  # per function, excluding the initial assignment and return
DEFAULT_REPS = 1
DEFAULT_PACKAGE_NAME = "bench_pkg"
DEFAULT_FUNCTION_COUNTS = (5, 50, 100, 200, 400, 600, 800)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Benchmark script compilation vs module compilation. "
            "Generates comparable .ds and .dmf sources and measures wall time."
        )
    )
    parser.add_argument(
        "--duckc",
        default="./build/bin/duckc",
        help="Path to duckc executable (default: ./build/bin/duckc).",
    )
    parser.add_argument(
        "--workdir",
        default="./src/compiler/repl/benchmarks/compile_bench",
        help="Directory for generated sources and artifacts.",
    )
    parser.add_argument(
        "--function-counts",
        type=int,
        nargs="+",
        default=list(DEFAULT_FUNCTION_COUNTS),
        help="Function counts to benchmark in one run.",
    )
    parser.add_argument(
        "--statements",
        type=int,
        default=DEFAULT_STATEMENTS,
        help="Number of statements per function.",
    )
    parser.add_argument(
        "--reps",
        type=int,
        default=DEFAULT_REPS,
        help="Number of repetitions per scenario.",
    )
    parser.add_argument(
        "--only",
        action="append",
        default=[],
        choices=["script-llvm", "script-dvm", "module-llvm", "module-dvm"],
        help="Limit benchmarks to specific scenario(s). Repeatable.",
    )
    parser.add_argument(
        "--keep-artifacts",
        action="store_true",
        help="Do not delete artifacts directories after each run.",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print compiler output and commands.",
    )
    return parser.parse_args()


def ensure_executable(path: Path) -> None:
    if not path.exists():
        raise FileNotFoundError(f"duckc not found: {path}")
    if not os.access(path, os.X_OK):
        raise PermissionError(f"duckc is not executable: {path}")


def generate_sources(
    output_dir: Path,
    function_count: int,
    statements_per_function: int,
    generator_kind: str,
    call_repetitions: int,
) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)

    functions = []
    for idx in range(function_count):
        body = [f"fun func_{idx}(a: i64) -> i64 = {{", "    var x: i64 = a;"]
        for step in range(statements_per_function):
            body.append(f"    x = x + {step + 1};")
        body.append("    return x;")
        body.append("}")
        functions.append("\n".join(body))

    def build_call_lines(indent: str) -> list[str]:
        lines = [f"{indent}var acc: i64 = 0;"]

        if generator_kind == "unique":
            # Call each function once per repetition (spread across functions).
            for repetition in range(call_repetitions):
                for idx in range(function_count):
                    lines.append(
                        f"{indent}acc = acc + func_{idx}({idx + repetition + 1});"
                    )
        elif generator_kind == "repeated":
            # Call the same function many times (stress repeated lookup).
            # Use func_0 as the target and call it `call_repetitions` times.
            for repetition in range(call_repetitions):
                lines.append(f"{indent}acc = acc + func_0({repetition + 1});")
        else:
            raise ValueError(f"Unknown generator kind: {generator_kind}")

        lines.append(f"{indent}builtin_output_i64(acc);")
        return lines

    calls = build_call_lines("")

    script_source = "\n\n".join(functions + ["\n".join(calls)])
    # Keep function definitions separated by blank lines, but keep the main body
    # compact (single newlines between statements). Previously we inlined the
    # call lines as individual list elements which then got double-newlined by
    # the outer join, producing empty lines between every call. Build the
    # main body as a single string block to avoid that.
    module_source = "\n\n".join(
        functions
        + [
            "fun main() -> i64 = {",
            "\n".join(build_call_lines("    ")),
            "    return acc;",
            "}",
        ]
    )

    (output_dir / "bench_script.ds").write_text(script_source, encoding="ascii")

    package_dir = output_dir / DEFAULT_PACKAGE_NAME
    package_dir.mkdir(parents=True, exist_ok=True)
    (package_dir / f"{DEFAULT_PACKAGE_NAME}.dmf").write_text(
        module_source, encoding="ascii"
    )
    return package_dir


def bench_label(function_count: int, generator_name: str) -> str:
    return f"functions_{function_count}/{generator_name}"


def format_progress(current: int, total: int, width: int = 24) -> str:
    if total <= 0:
        return f"[{'-' * width}] 0/0"

    clamped_current = max(0, min(current, total))
    filled = int(round(width * clamped_current / total))
    filled = min(filled, width)
    return f"[{'#' * filled}{'-' * (width - filled)}] {clamped_current}/{total}"


def run_command(cmd: list[str], verbose: bool) -> float:
    if verbose:
        print(" ".join(cmd))
    start = time.perf_counter()
    try:
        subprocess.run(
            cmd,
            check=True,
            stdout=None if verbose else subprocess.PIPE,
            stderr=None if verbose else subprocess.PIPE,
            text=not verbose,
        )
    except subprocess.CalledProcessError as err:
        if not verbose:
            if err.stdout:
                print(err.stdout)
            if err.stderr:
                print(err.stderr)
        raise
    return time.perf_counter() - start


def bench_scenario(
    name: str,
    cmd: list[str],
    reps: int,
    artifacts_dir: Path,
    keep_artifacts: bool,
    verbose: bool,
) -> list[float]:
    times = []
    for rep in range(reps):
        print(
            f"    {format_progress(rep + 1, reps)} {name} rep {rep + 1}/{reps}",
            file=sys.stdout,
            flush=True,
        )
        artifacts_dir.mkdir(parents=True, exist_ok=True)
        run_cmd = cmd + [
            "--artifact-location",
            str(artifacts_dir),
        ]
        elapsed = run_command(run_cmd, verbose)
        times.append(elapsed)
        if not keep_artifacts:
            shutil.rmtree(artifacts_dir, ignore_errors=True)
    return times


def main() -> int:
    args = parse_args()

    duckc_path = Path(args.duckc)
    ensure_executable(duckc_path)

    workdir = Path(args.workdir)
    sources_dir = workdir / "sources"
    results_dir = workdir / "results"
    results_dir.mkdir(parents=True, exist_ok=True)

    generator_specs = ["unique", "repeated"]

    scenarios = {
        "script-llvm": lambda script_path, module_path: [
            str(duckc_path),
            "compile_script",
            str(script_path),
            "--workers",
            "1",
        ],
        "script-dvm": lambda script_path, module_path: [
            str(duckc_path),
            "compile_script",
            str(script_path),
            "--dvm-backend",
            "--workers",
            "1",
        ],
        "module-llvm": lambda script_path, module_path: [
            str(duckc_path),
            "compile_package",
            str(module_path),
            "--name",
            DEFAULT_PACKAGE_NAME,
            "--output-file-name",
            "bench_module_llvm",
            "--no-incremental",
            "--workers",
            "1",
        ],
        "module-dvm": lambda script_path, module_path: [
            str(duckc_path),
            "compile_package",
            str(module_path),
            "--name",
            DEFAULT_PACKAGE_NAME,
            "--output-file-name",
            "bench_module_dvm",
            "--dvm-backend",
            "--no-incremental",
            "--workers",
            "1",
        ],
    }

    selected = args.only if args.only else list(scenarios.keys())

    summary = []
    total_benchmarks = len(args.function_counts) * len(generator_specs) * len(selected)
    completed_benchmarks = 0

    print(
        f"Running {total_benchmarks} benchmark combinations across "
        f"{len(args.function_counts)} function-count setting(s) and "
        f"{len(generator_specs)} generator(s).",
        flush=True,
    )

    for function_count in args.function_counts:
        for generator_name in generator_specs:
            # unique: call each function once; repeated: call same function N times
            if generator_name == "unique":
                call_repetitions = 1
            else:
                call_repetitions = function_count

            bench_root = bench_label(function_count, generator_name)
            source_dir = sources_dir / bench_root
            module_dir = generate_sources(
                source_dir,
                function_count,
                args.statements,
                generator_name,
                call_repetitions,
            )
            script_path = source_dir / "bench_script.ds"

            for scenario in selected:
                completed_benchmarks += 1
                print(
                    f"\n{format_progress(completed_benchmarks, total_benchmarks)} "
                    f"{bench_root} -> {scenario}",
                    flush=True,
                )
                artifacts_dir = results_dir / bench_root / scenario
                times = bench_scenario(
                    scenario,
                    scenarios[scenario](script_path, module_dir),
                    args.reps,
                    artifacts_dir,
                    args.keep_artifacts,
                    args.verbose,
                )
                summary.append(
                    {
                        "function_count": function_count,
                        "generator": generator_name,
                        "call_repetitions": call_repetitions,
                        "scenario": scenario,
                        "reps": len(times),
                        "min_s": min(times),
                        "avg_s": sum(times) / len(times),
                    }
                )

    csv_path = results_dir / "bench_results.csv"
    with csv_path.open("w", newline="", encoding="ascii") as csv_file:
        writer = csv.DictWriter(
            csv_file,
            fieldnames=[
                "function_count",
                "generator",
                "call_repetitions",
                "scenario",
                "reps",
                "min_s",
                "avg_s",
            ],
        )
        writer.writeheader()
        writer.writerows(summary)

    print("\nResults:")
    for row in summary:
        print(
            f"- n={row['function_count']} {row['generator']} {row['scenario']}: "
            f"reps={row['reps']} min={row['min_s']:.4f}s avg={row['avg_s']:.4f}s"
        )
    print(f"\nCSV written to: {csv_path}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
