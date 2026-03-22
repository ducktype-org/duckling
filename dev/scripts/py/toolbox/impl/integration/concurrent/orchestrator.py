from pathlib import Path

from ..test_loader import load_tests
from ..tester import tester_impl
from ..utils import print_success
from ...helpers import exit_with_error, get_dev_directory, log_info
from .config import load_default_concurrent_config
from .module_copy import duplicate_package_modules, remove_files
from .runner import run_concurrent_compile_determinism
from .selection import TestRef, collect_eligible_cases


# In concurrent determinism mode we verify that compiling the same package with the same backend
# (LLVM or DVM) produces identical .o/.dbc artifacts for 1 worker and 3 workers.
#
# High-level flow:
# 1) select eligible compile_package test cases,
# 2) prepare temporary duplicated modules,
# 3) run compile phase in single-thread and concurrent mode,
# 4) compare artifact snapshots,
# 5) cleanup temporary files in finally.


def concurrent_tester_impl(
    clean: bool,
    dry: bool,
    filter: str,
    fail_fast: bool,
    verbose: bool,
    log_file: str | Path,
    build_dir: str,
    duckc_worker_count: int,
):
    config = load_default_concurrent_config()

    log_file = Path(log_file)
    if log_file.exists() and not dry:
        log_file.unlink()

    def make_user_values(worker_count: int) -> dict[str, str]:
        return {
            "build_dir": str(Path(build_dir).absolute()),
            "dev_dir": str(get_dev_directory()),
            "duckc_worker_count": str(worker_count),
        }

    concurrent_test_set = load_tests("integration_tests", user_values=make_user_values(worker_count=3))

    selected_case_paths: set[str] = set()
    selected_tests: set[TestRef] = set()
    selected_backends: dict[str, str] = {}
    collect_eligible_cases(
        concurrent_test_set,
        [],
        filter,
        config.blacklist,
        selected_case_paths,
        selected_tests,
        selected_backends,
    )

    if not selected_case_paths:
        exit_with_error("No eligible compile_package LLVM/DVM integration test cases found.")

    llvm_cases = sum(1 for backend in selected_backends.values() if backend == "llvm")
    dvm_cases = sum(1 for backend in selected_backends.values() if backend == "dvm")
    log_info(
        "Concurrent compile determinism mode selected cases: "
        f"{len(selected_case_paths)} total ({llvm_cases} llvm, {dvm_cases} dvm)."
    )

    if duckc_worker_count != 3:
        log_info(
            "Concurrent mode overrides duckc worker count to 3. "
            f"Ignoring provided value: {duckc_worker_count}."
        )

    if clean:
        tester_impl(
            clean=clean,
            dry=dry,
            filter=filter,
            fail_fast=fail_fast,
            verbose=verbose,
            log_file=log_file,
            build_dir=build_dir,
            duckc_worker_count=3,
            allowed_case_paths=selected_case_paths,
        )
        return

    created_files: list[Path] = []
    if not dry:
        log_info("Preparing temporary duplicated .dmf modules for concurrent itests...")
        for test_ref in sorted(selected_tests, key=lambda ref: str(ref.cwd / ref.name)):
            package_dir = test_ref.cwd / "duck_modules" / test_ref.name
            created_files += duplicate_package_modules(package_dir, config.copy_count, True)

    try:
        single_thread_set = load_tests("integration_tests", user_values=make_user_values(worker_count=1))
        concurrent_set = load_tests("integration_tests", user_values=make_user_values(worker_count=3))

        succeeded, failed, disabled = run_concurrent_compile_determinism(
            selected_case_paths=selected_case_paths,
            single_thread_set=single_thread_set,
            concurrent_set=concurrent_set,
            dry=dry,
            fail_fast=fail_fast,
            verbose=verbose,
            log_file=log_file,
        )

        if dry:
            return

        total_test_count = len(succeeded) + len(failed) + len(disabled)
        print(f"Ran test count: {total_test_count}")
        print(f" - Succeeded: {len(succeeded)}")
        print(f" - Disabled:  {len(disabled)}")
        print(f" - Failed:    {len(failed)}")

        if failed:
            failed_tests = map(lambda x: " - " + x, failed)
            exit_with_error(
                f"{'(Fail fast) ' if fail_fast else ''}Failed tests:\n{chr(10).join(failed_tests)}\n"
                + f"Please see log file '{log_file.absolute()}' for more info."
            )

        print_success("All concurrent compile determinism tests have run successfully!")
    finally:
        if created_files:
            log_info("Cleaning up temporary duplicated .dmf modules...")
            remove_files(created_files)
