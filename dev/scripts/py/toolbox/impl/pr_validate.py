from ..impl.test import test_impl
from .helpers import (
    bash_command,
    exit_with_error,
)
from .duck_linter import duck_linter_impl
from .cpp_linter import cpp_linter_impl
from .issue_checker import issue_checker_impl
from .todo_validate import todo_validate_impl
from .integration.tester import tester_impl, DEFAULT_LOG_FILE_PATH


def pr_validate_impl(
    clang_tidy_path: str,
    clang_format_path: str,
    build_dir: str,
    thread_count: int,
    auto_fix: bool,
    no_fix: bool,
):
    """
    Perform PR validation steps.
    First tries to run steps that are quick to run, then progresses to the
    more time-consuming ones.
    """

    # Step 1 - duck linter
    if not duck_linter_impl(auto_fix=auto_fix, no_fix=no_fix):
        exit_with_error("Duck linter has failed")

    # Step 2 - validate !todos and !fixmes
    if not todo_validate_impl():
        exit_with_error("T" + "ODO validation has failed")

    # Step 3 - issue checker
    if not issue_checker_impl([]):
        exit_with_error("Issue checker has failed")

    # Step 4 - Run formatting checker
    _, clang_format_failed = cpp_linter_impl(
        clang_tidy_path=None,
        clang_format_path=clang_format_path,
        build_dir=build_dir,
        thread_count=thread_count,
        auto_fix=auto_fix,
        no_fix=no_fix,
    )

    if clang_format_failed:
        exit_with_error(
            f"C++ formatting check failed. Please run `./scripts/formatting/format_repo_cpp.sh {clang_format_path}` or fix the issues manually."
        )

    # Step 5 - build
    bash_command(
        f"cmake --build {build_dir} -- -j {thread_count} all build_all_tests build_all_playgrounds"
    )

    # Step 6 - test
    test_impl(build_dir=build_dir, thread_count=thread_count, timeout=60)

    # Step 7 - integration tests
    tester_impl(
        clean=False,
        dry=False,
        filter="",
        fail_fast=False,
        verbose=False,
        log_file=DEFAULT_LOG_FILE_PATH,
        build_dir=build_dir,
        jobs=thread_count,
    )

    # Step 8 - clang-tidy
    clang_tidy_failed, _ = cpp_linter_impl(
        clang_tidy_path=clang_tidy_path,
        clang_format_path=None,
        build_dir=build_dir,
        thread_count=thread_count,
        auto_fix=auto_fix,
        no_fix=no_fix,
    )

    if clang_tidy_failed:
        exit_with_error("C++ clang-tidy check has failed")
