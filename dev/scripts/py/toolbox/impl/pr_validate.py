from .helpers import (
    bash_command,
    exit_with_error,
)
from .duck_linter import duck_linter_impl
from .cpp_linter import cpp_linter_impl
from .issue_checker import issue_checker_impl
from .integration.tester import tester_impl, DEFAULT_LOG_FILE_PATH


def pr_validate_impl(clang_tidy_path: str, clang_format_path: str, build: str):
    
    # Step 1 - build
    bash_command(f"cmake --build {build} -- all build_all_tests build_all_playgrounds")

    # Step 2 - test
    bash_command(f"cmake --build {build} -- test")

    # Step 3 - integration tests
    tester_impl(
        clean=False,
        dry=False,
        filter="",
        fail_fast=False,
        verbose=False,
        log_file=DEFAULT_LOG_FILE_PATH,
        build_dir=build)

    # Step 4 - duck linter
    if not duck_linter_impl():
        exit_with_error("Duck linter has failed")

    # Step 5 - cpp linter
    clang_tidy_failed, clang_format_failed = cpp_linter_impl(
        clang_tidy_path=clang_tidy_path,
        clang_format_path=clang_format_path,
        build=build,
    )

    # Step 6 - issue checker
    if not issue_checker_impl([]):
        exit_with_error("Issue checker has failed")
    
    if clang_tidy_failed or clang_format_failed:
        exit_with_error(
            f"CPP linter has failed: {clang_tidy_failed=} {clang_format_failed=}"
        )