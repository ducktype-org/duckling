#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
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
# 1) duplicate every .dmf module file in every package under test (default x3),
#    so there is enough work to exercise concurrent modification paths
#    and force parallel processing of identical function bodies,
# 2) rewrite duplicated files so they can coexist in one package:
#    - rename `fun main(...)` to `fun main_copyN(...)` because the linker
#      requires exactly one main function per package,
#    - rewrite imports in copied main-module files from `import child...`
#      to `import main.child...` because copied .dmf files become
#      sub-modules of the original, so their import paths must be adjusted,
# 3) compile the same package(s) twice into a local `build` dir:
#    - first with 1 worker to get a deterministic baseline,
#    - then with concurrent workers — if single-threaded output matches
#      multi-threaded output the compiler is deterministic,
# 4) compare sha256 hashes of produced .o/.dbc artifacts to verify
#    byte-identical output regardless of thread scheduling order,
# 5) clean temporary duplicated modules and temporary build artifacts,
# 6) return 0 on success, otherwise return 1 and print error details to stderr.
#
# The script supports two modes: single-package mode (--module-name selects a
# package under duck_modules/), and manifest mode (--manifest points to a
# compile_packages JSON manifest).


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


# Returns package directories from a compile_packages manifest.
def get_manifest_package_dirs(manifest_path: Path) -> list[Path]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    packages = manifest.get("packages")
    if not isinstance(packages, list) or not packages:
        raise ValueError("Manifest must define a non-empty 'packages' array.")

    package_dirs: list[Path] = []
    seen: set[str] = set()
    for package in packages:
        if not isinstance(package, dict) or "path" not in package:
            raise ValueError("Manifest package entries must define a 'path'.")
        package_dir = Path(package["path"])
        key = str(package_dir)
        if key in seen:
            continue
        if not package_dir.exists():
            raise ValueError(f"Manifest package path not found: {package_dir}")
        seen.add(key)
        package_dirs.append(package_dir)

    return package_dirs


# Duplicates all packages referenced by the manifest.
def duplicate_manifest_packages(manifest_path: Path, copy_count: int) -> list[Path]:
    created_files: list[Path] = []
    for package_dir in get_manifest_package_dirs(manifest_path):
        created_files.extend(duplicate_package_modules(package_dir, copy_count))
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
def collect_artifact_snapshot(build_dir: Path, suffixes: set[str] | None = None) -> dict[str, str]:
    if not build_dir.exists():
        return {}

    if suffixes is None:
        suffixes = {".o", ".dbc"}

    files = sorted(file for file in build_dir.rglob("*") if file.is_file() and file.suffix in suffixes)
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
    source_group = parser.add_mutually_exclusive_group(required=True)
    source_group.add_argument("--module-name", help="Single-package mode: name of the package under duck_modules/.")
    source_group.add_argument("--manifest", help="Manifest mode: path to a compile_packages JSON manifest.")
    parser.add_argument("--duckc-worker-count", type=int, required=True)
    parser.add_argument("--compile-options", default="")
    # In manifest mode the backend is determined per-task by the manifest, so
    # --backend is optional there. Kept required-ish for compatibility: when
    # provided in manifest mode it is silently ignored.
    parser.add_argument("--backend", choices=["dvm", "llvm"])
    parser.add_argument("--copy-count", type=int, default=3)
    return parser.parse_args()


# Recreates local build directory for one compile pass.
def clean_build_dir(build_dir: Path) -> None:
    shutil.rmtree(build_dir, ignore_errors=True)
    build_dir.mkdir(parents=True, exist_ok=True)


# Executes duckc compile_packages on a manifest and captures process output.
def compile_packages_manifest(
    duckc_path: str,
    manifest_path: str,
    workers: int,
    compile_options: str,
) -> tuple[int, str, str]:
    command = [
        duckc_path,
        "compile_packages",
        manifest_path,
        "-w",
        str(workers),
        "-a",
        "build",
    ]

    if compile_options.strip():
        command.extend(shlex.split(compile_options))

    result = subprocess.run(command, capture_output=True, text=True)
    return result.returncode, result.stdout, result.stderr


# Runs one compile_packages pass and returns success flag, snapshot and error details.
def run_once_manifest(
    duckc_path: str,
    manifest_path: str,
    workers: int,
    compile_options: str,
    build_dir: Path,
) -> tuple[bool, dict[str, str], str]:
    clean_build_dir(build_dir)
    code, out, err = compile_packages_manifest(
        duckc_path=duckc_path,
        manifest_path=manifest_path,
        workers=workers,
        compile_options=compile_options,
    )
    if code != 0:
        details = (
            f"compile_packages failed for workers={workers} (exit code {code})\n"
            f"stdout:\n{out}\n"
            f"stderr:\n{err}\n"
        )
        return False, {}, details

    # In manifest mode tasks may emit .o, .dbc, .a, .exe — hash them all.
    snapshot = collect_artifact_snapshot(build_dir, {".o", ".dbc", ".a", ".exe"})
    return True, snapshot, ""


# Manifest-mode determinism check: compile_packages with 1 vs N workers,
# diff produced artifacts. Uses the same source-duplication trick as
# single-package mode to stress concurrent compilation.
def run_manifest_mode(args: argparse.Namespace) -> int:
    manifest_path = Path(args.manifest)
    if not manifest_path.exists():
        print(
            f"[determinism-check] Manifest not found: {manifest_path}",
            file=sys.stderr,
        )
        return 1

    concurrent_workers = args.duckc_worker_count if args.duckc_worker_count > 1 else 3
    build_dir = Path("build")
    created_files: list[Path] = []

    try:
        created_files = duplicate_manifest_packages(manifest_path, args.copy_count)
    except ValueError as error:
        print(f"[determinism-check] {error}", file=sys.stderr)
        return 1

    try:
        ok_single, single_snapshot, single_error = run_once_manifest(
            duckc_path=args.duckc_path,
            manifest_path=str(manifest_path),
            workers=1,
            compile_options=args.compile_options,
            build_dir=build_dir,
        )
        if not ok_single:
            print(
                "[determinism-check] Compilation failed (single-worker pass).\n" + single_error,
                file=sys.stderr,
            )
            return 1

        ok_concurrent, concurrent_snapshot, concurrent_error = run_once_manifest(
            duckc_path=args.duckc_path,
            manifest_path=str(manifest_path),
            workers=concurrent_workers,
            compile_options=args.compile_options,
            build_dir=build_dir,
        )
        if not ok_concurrent:
            print(
                "[determinism-check] Compilation failed (concurrent pass).\n" + concurrent_error,
                file=sys.stderr,
            )
            return 1

        if not single_snapshot and not concurrent_snapshot:
            print(
                "[determinism-check] No compiled artifacts were produced; cannot perform determinism check.\n"
                "Expected at least one .o/.dbc/.a/.exe artifact.",
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
            "[determinism-check] Deterministic artifacts verified for manifest "
            f"'{manifest_path}' (workers 1 vs {concurrent_workers})."
        )
        return 0
    finally:
        remove_files(created_files)
        shutil.rmtree(build_dir, ignore_errors=True)


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

    if args.manifest:
        return run_manifest_mode(args)

    if not args.backend:
        print(
            "[determinism-check] --backend is required in single-package mode.",
            file=sys.stderr,
        )
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