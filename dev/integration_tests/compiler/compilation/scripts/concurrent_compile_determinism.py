#!/usr/bin/env python3

from __future__ import annotations

import argparse
from hashlib import sha256
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys


# Deterministic compile pre-check used by integration tests.
#
# The goal is to verify that concurrent compilation produces byte-identical
# artifacts to single-threaded compilation. Duplicating modules increases
# the workload so that worker threads naturally process the same functions
# in parallel, exposing any non-determinism from thread scheduling.
#
# Beyond determinism, this test indirectly helps detect concurrency bugs:
# non-deterministic output can be a symptom of data races, and running
# a heavier concurrent workload under a test timeout helps surface
# deadlocks. Neither check is 100% reliable, but together they catch
# a meaningful class of threading issues.
#
# High-level flow:
# 1) duplicate every .dmf module file in one selected package (default x3),
#    so there is enough work to exercise concurrent modification paths
#    and force parallel processing of identical function bodies,
# 2) rewrite duplicated files so they can coexist in one package:
#    - rename `fun main(...)` to `fun main_copyN(...)` because the linker
#      requires exactly one main function per package,
#    - rewrite imports in copied main-module files from `import child...`
#      to `import main.child...` because copied .dmf files become
#      sub-modules of the original, so their import paths must be adjusted,
# 3) compile the same package twice into a local `build` dir:
#    - first with 1 worker to get a deterministic baseline,
#    - then with concurrent workers — if single-threaded output matches
#      multi-threaded output the compiler is deterministic,
# 4) compare sha256 hashes of produced .o/.dbc artifacts to verify
#    byte-identical output regardless of thread scheduling order,
# 5) clean temporary duplicated modules and temporary build artifacts,
# 6) return 0 on success, otherwise return 1 and print error details to stderr.
#
# The script intentionally operates on one package selected by --module-name
# (duck_modules/<module-name>) so tests can scope checks to a single case.


GENERATED_COPY_STEM_RE = re.compile(r"^concurrent_copy\d+_.+")
IMPORT_RE = re.compile(r"^(\s*import\s+)([A-Za-z_]\w*)(?=[\s\.;]|$)")
MAIN_FUN_RE = re.compile(r"(\bfun\s+)main(\s*\()")


# Returns True when file is a generated temporary copy.
def is_copy_file(path: Path) -> bool:
    return path.suffix == ".dmf" and GENERATED_COPY_STEM_RE.match(path.stem) is not None


# Lists original .dmf files that should be duplicated.
def iter_original_dmf_files(package_dir: Path) -> list[Path]:
    return sorted([file for file in package_dir.rglob("*.dmf") if not is_copy_file(file)])


# Detects child module names for import rewriting in copied main modules.
def get_child_module_names(main_module_file: Path) -> set[str]:
    parent_dir = main_module_file.parent
    main_name = main_module_file.stem
    child_names = set()

    for child in parent_dir.iterdir():
        if child.is_file() and child.suffix == ".dmf" and not is_copy_file(child):
            if child.stem != main_name:
                child_names.add(child.stem)
            continue

        if child.is_dir() and any(
            nested.suffix == ".dmf" and not is_copy_file(nested) for nested in child.rglob("*.dmf")
        ):
            child_names.add(child.name)

    return child_names


# Rewrites one import line to keep copied module imports valid.
def rewrite_import_line_if_needed(
    line: str,
    main_module_name: str,
    child_module_names: set[str],
) -> str:
    newline = "\n" if line.endswith("\n") else ""
    line_without_newline = line[:-1] if newline else line

    match = IMPORT_RE.match(line_without_newline)
    if not match:
        return line

    first_segment = match.group(2)
    if first_segment not in child_module_names:
        return line
    if first_segment == main_module_name:
        return line

    first_segment_start, first_segment_end = match.span(2)
    rewritten = (
        f"{line_without_newline[:first_segment_start]}"
        f"{main_module_name}.{line_without_newline[first_segment_start:first_segment_end]}"
        f"{line_without_newline[first_segment_end:]}"
    )
    return f"{rewritten}{newline}"


# Applies main rename and import rewrites to one copied file content.
def rewrite_for_copy(source_text: str, source_file: Path, copy_index: int) -> str:
    rewritten = MAIN_FUN_RE.sub(rf"\1main_copy{copy_index}\2", source_text)

    if source_file.stem != source_file.parent.name:
        return rewritten

    main_module_name = source_file.stem
    child_module_names = get_child_module_names(source_file)

    rewritten_lines = [
        rewrite_import_line_if_needed(line, main_module_name, child_module_names)
        for line in rewritten.splitlines(keepends=True)
    ]
    return "".join(rewritten_lines)


# Duplicates all package modules and returns created temp file paths.
def duplicate_package_modules(package_dir: Path, copy_count: int) -> list[Path]:
    created_files: list[Path] = []

    originals = iter_original_dmf_files(package_dir)
    for source_file in originals:
        source_text = source_file.read_text(encoding="utf-8")
        sanitized_source_stem = source_file.stem.replace("-", "_")
        for copy_index in range(1, copy_count + 1):
            target = source_file.with_name(f"concurrent_copy{copy_index}_{sanitized_source_stem}.dmf")
            rewritten = rewrite_for_copy(source_text, source_file, copy_index)
            target.write_text(rewritten, encoding="utf-8")
            created_files.append(target)

    return created_files


# Removes temporary duplicated files.
def remove_files(paths: list[Path]) -> None:
    for file in reversed(paths):
        try:
            if file.exists():
                file.unlink()
        except OSError as error:
            print(f"[determinism-check] Failed to remove temp file '{file}': {error}", file=sys.stderr)


# Collects sha256 hashes for produced object/bytecode artifacts.
def collect_artifact_snapshot(build_dir: Path) -> dict[str, str]:
    if not build_dir.exists():
        return {}

    files = sorted(file for file in build_dir.rglob("*") if file.is_file() and file.suffix in {".o", ".dbc"})
    snapshot: dict[str, str] = {}
    for file in files:
        rel = file.relative_to(build_dir).as_posix()
        snapshot[rel] = sha256(file.read_bytes()).hexdigest()
    return snapshot


# Compares two artifact snapshots and returns human-readable differences.
def diff_snapshots(single: dict[str, str], concurrent: dict[str, str]) -> list[str]:
    diffs: list[str] = []

    single_paths = set(single.keys())
    concurrent_paths = set(concurrent.keys())

    missing_in_concurrent = sorted(single_paths - concurrent_paths)
    extra_in_concurrent = sorted(concurrent_paths - single_paths)

    if missing_in_concurrent:
        diffs.append("Missing in concurrent build: " + ", ".join(missing_in_concurrent[:10]))
    if extra_in_concurrent:
        diffs.append("Extra in concurrent build: " + ", ".join(extra_in_concurrent[:10]))

    changed = sorted(path for path in (single_paths & concurrent_paths) if single[path] != concurrent[path])
    if changed:
        diffs.append("Content differs: " + ", ".join(changed[:10]))

    return diffs


# Parses script CLI arguments.
def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Checks compile_package determinism for one test module by duplicating source modules, "
            "compiling with 1 and concurrent workers, and comparing produced .o/.dbc artifacts."
        )
    )
    parser.add_argument("--duckc-path", required=True)
    parser.add_argument("--module-name", required=True)
    parser.add_argument("--duckc-worker-count", type=int, required=True)
    parser.add_argument("--compile-options", default="")
    parser.add_argument("--backend", choices=["dvm", "llvm"], required=True)
    parser.add_argument("--copy-count", type=int, default=3)
    return parser.parse_args()


# Recreates local build directory for one compile pass.
def clean_build_dir(build_dir: Path) -> None:
    shutil.rmtree(build_dir, ignore_errors=True)
    build_dir.mkdir(parents=True, exist_ok=True)


# Executes duckc compile_package and captures process output.
def compile_package(
    duckc_path: str,
    module_name: str,
    workers: int,
    compile_options: str,
    backend: str,
) -> tuple[int, str, str]:
    command = [
        duckc_path,
        "compile_package",
        f"duck_modules/{module_name}",
        "-w",
        str(workers),
        "-a",
        "build",
        "-n",
        f"package_{module_name}",
    ]

    if compile_options.strip():
        command.extend(shlex.split(compile_options))

    if backend == "dvm":
        command.append("--dvm-backend")

    result = subprocess.run(command, capture_output=True, text=True)
    return result.returncode, result.stdout, result.stderr


# Runs one compile pass and returns success flag, snapshot and error details.
def run_once(
    duckc_path: str,
    module_name: str,
    workers: int,
    compile_options: str,
    backend: str,
    build_dir: Path,
) -> tuple[bool, dict[str, str], str]:
    clean_build_dir(build_dir)
    code, out, err = compile_package(
        duckc_path=duckc_path,
        module_name=module_name,
        workers=workers,
        compile_options=compile_options,
        backend=backend,
    )
    if code != 0:
        details = (
            f"compile_package failed for workers={workers} (exit code {code})\n"
            f"stdout:\n{out}\n"
            f"stderr:\n{err}\n"
        )
        return False, {}, details

    snapshot = collect_artifact_snapshot(build_dir)
    return True, snapshot, ""


# Orchestrates the deterministic pre-check workflow end-to-end.
def main() -> int:
    args = parse_args()

    if args.duckc_worker_count < 1:
        print("[determinism-check] Invalid duckc worker count (must be >= 1).", file=sys.stderr)
        return 1
    if args.copy_count < 1:
        print("[determinism-check] Invalid copy count (must be >= 1).", file=sys.stderr)
        return 1

    package_dir = Path("duck_modules") / args.module_name
    if not package_dir.exists():
        print(
            f"[determinism-check] Module package not found: {package_dir}",
            file=sys.stderr,
        )
        return 1

    concurrent_workers = args.duckc_worker_count if args.duckc_worker_count > 1 else 3
    build_dir = Path("build")
    created_files: list[Path] = []

    try:
        created_files = duplicate_package_modules(package_dir, args.copy_count)

        ok_single, single_snapshot, single_error = run_once(
            duckc_path=args.duckc_path,
            module_name=args.module_name,
            workers=1,
            compile_options=args.compile_options,
            backend=args.backend,
            build_dir=build_dir,
        )
        if not ok_single:
            print(
                "[determinism-check] Compilation failed (single-worker pass).\n"
                + single_error,
                file=sys.stderr,
            )
            return 1

        ok_concurrent, concurrent_snapshot, concurrent_error = run_once(
            duckc_path=args.duckc_path,
            module_name=args.module_name,
            workers=concurrent_workers,
            compile_options=args.compile_options,
            backend=args.backend,
            build_dir=build_dir,
        )
        if not ok_concurrent:
            print(
                "[determinism-check] Compilation failed (concurrent pass).\n"
                + concurrent_error,
                file=sys.stderr,
            )
            return 1

        if not single_snapshot and not concurrent_snapshot:
            print(
                "[determinism-check] No compiled artifacts were produced; cannot perform determinism check.\n"
                f"Expected at least one .o/.dbc artifact for backend '{args.backend}'.",
                file=sys.stderr,
            )
            return 1

        diffs = diff_snapshots(single_snapshot, concurrent_snapshot)
        if diffs:
            print(
                "[determinism-check] Compilation is not deterministic and did not compile consistently.\n"
                + "\n".join(diffs),
                file=sys.stderr,
            )
            return 1

        print(
            "[determinism-check] Deterministic artifacts verified for module "
            f"'{args.module_name}' (workers 1 vs {concurrent_workers})."
        )
        return 0
    finally:
        remove_files(created_files)
        shutil.rmtree(build_dir, ignore_errors=True)


# Script entry point.
if __name__ == "__main__":
    sys.exit(main())