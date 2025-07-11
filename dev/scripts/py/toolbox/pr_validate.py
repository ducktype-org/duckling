from .helpers import (
    bash_command,
    exit_with_error,
)
from click import option

from .duck_linter import impl as duck_linter_impl
from .cpp_linter import impl as cpp_linter_impl
from .issue_checker import impl as issue_checker_impl
from .integration.tester import impl as integration_tests_impl, DEFAULT_LOG_FILE_PATH


def impl(clang_tidy_path: str, clang_format_path: str, build: str):
    
    # Step 1 - build
    bash_command(f"cmake --build {build} -- all build_all_tests build_all_playgrounds")

    # Step 2 - test
    bash_command(f"cmake --build {build} -- test")

    # Step 3 - integration tests
    integration_tests_impl(
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

def tidy(func):
    return option(
        "-t",
        "--tidy",
        "clang_tidy_path",
        prompt="clang-tidy path",
        help="Path to clang-tidy, ex. /usr/bin/clang-tidy-19 or clang-tidy",
        default="clang-tidy-19",
    )(func)

def format(func):
    return option(
        "-f",
        "--format",
        "clang_format_path",
        prompt="clang-format path",
        help="Path to clang-format, ex. /usr/bin/clang-format-19 or clang-format",
        default="clang-format-19",
    )(func)

def build(func):
    return option(
        "-b",
        "--build",
        prompt="build folder",
        help="Path to build folder with compile_commands.json",
        default="build",
    )(func)