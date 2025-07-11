#!/usr/bin/env python3

import pathlib
import sys

import click


from scripts.py.toolbox.helpers import exit_with_error

from scripts.py.toolbox.integration import tester as integration_tests_lib
from scripts.py.toolbox import (
    setup_build as setup_build_lib,
    setup_venv as setup_venv_lib,
    download_binaries as download_binaries_lib,
    init as init_lib,
    coverage as coverage_lib,
    clean_init as clean_init_lib,
    docs as docs_lib,
    test as test_lib,
    download_llvm as download_llvm_lib,
    install_llvm as install_llvm_lib,
    cpp_linter as cpp_linter_lib,
    duck_linter as duck_linter_lib,
    todo_counter as todo_counter_lib,
    pr_validate as pr_validate_lib,
    issue_checker as issue_checker_lib,

)

DATA_USER = "dev"
# @FUTURE: change this password and hide it:
DATA_PASS = "7ocwXWOAwg="


@click.group()
def cli():
    pass


@cli.command()
@setup_build_lib.build_dir
@setup_build_lib.build_system
@setup_build_lib.build_type
@setup_build_lib.build_doc
@setup_build_lib.cxx_compiler
@setup_build_lib.cc_compiler
@setup_build_lib.ccache
@setup_build_lib.coverage
@setup_build_lib.gcov_version
@setup_build_lib.linker
def setup_build(*args, **kwargs):
    """Makes a build folder"""
    setup_build_lib.impl(*args, **kwargs)


@cli.command()
def setup_venv():
    """Setups python virtual environment, download dependencies"""
    setup_venv_lib.impl()


@cli.command()
@download_binaries_lib.force
@download_binaries_lib.single
def download_binaries(*args, **kwargs):
    """Downloads necessary binary files from the internet"""
    download_binaries_lib.impl(*args, **kwargs)


@cli.command()
def init():
    """A general repo setup, performs downloading of submodules and binaries, creates a python venv, etc..."""
    init_lib.impl()


@cli.command()
@coverage_lib.build_dir
@coverage_lib.thread_count
def coverage(*args, **kwargs):
    """Builds and runs coverage inside given build directory.
    This directory has to have coverage enabled"""
    coverage_lib.impl(*args, **kwargs)


@cli.command()
@clean_init_lib.yes_flag
def clean_init():
    """Removes things created by init."""
    clean_init_lib.impl()

@cli.command()
@docs_lib.build_dir
def docs(*args, **kwargs):
    """Builds a documentation for the project and opens it in the browser"""
    docs_lib.impl(*args, **kwargs)


@cli.command()
@test_lib.build_dir
@test_lib.memcheck
def test(*args, **kwargs):
    """Performs tests of the code"""
    test_lib.impl(*args, **kwargs)


@cli.command()
@download_llvm_lib.confirm
@download_llvm_lib.version
@download_llvm_lib.os
@download_llvm_lib.architecture
def download_llvm(*args, **kwargs):
    """Downloads the specified version of LLVM."""
    download_llvm_lib.impl(*args, **kwargs)


@cli.command()
@install_llvm_lib.version
@install_llvm_lib.ram
@install_llvm_lib.cxx_compiler
@install_llvm_lib.c_compiler
@install_llvm_lib.linker
@install_llvm_lib.build_tool
@install_llvm_lib.targets
@install_llvm_lib.source_dir
@install_llvm_lib.use_old_dir
def install_llvm(*args, **kwargs):
    """Compiles LLVM from source with specified options.

    This command will download LLVM source code, build it with the specified options,
    and install it to the 'scripts/downloads/installed' directory.
    Important! Is is advised to try to use the LLVM from your
    distribution (e.g. apt install llvm-19) first.
    """
    install_llvm_lib.impl(*args, **kwargs)


@cli.command()
@cpp_linter_lib.tidy
@cpp_linter_lib.format
@cpp_linter_lib.build
@cpp_linter_lib.threads
@cpp_linter_lib.branch
@cpp_linter_lib.all_flag
@cpp_linter_lib.no_merge_base
def cpp_linter(*args, **kwargs):
    """Simulates clang-tidy and clang-format as if in a workflow.

    It compares the current branch's working tree with the most recent common ancestor shared with the 'main' branch (called the merge base).
    """
    clang_tidy_failed, clang_format_failed = cpp_linter_lib.impl(*args, **kwargs)
    if clang_tidy_failed or clang_format_failed:
        exit_with_error(
            f"Linter has failed because: {clang_tidy_failed=}, {clang_format_failed=}"
        )


@cli.command()
@duck_linter_lib.all_flag
@duck_linter_lib.branch
@duck_linter_lib.verbose
@duck_linter_lib.no_merge_base
def duck_linter(*args, **kwargs):
    """Check for violations of
    some of the C++ coding guidelines for Duckling project.
    Current checks:
    * relative import check

    For details see dev-guides.
    """
    passed = duck_linter_lib.impl(*args, **kwargs)
    if not passed:
        exit_with_error("Linting failed.")


@cli.command()
@integration_tests_lib.clean
@integration_tests_lib.dry
@integration_tests_lib.filter
@integration_tests_lib.fail_fast
@integration_tests_lib.verbose
@integration_tests_lib.log_file
@integration_tests_lib.build_dir
def itest(*args, **kwargs):
    """Runs integration tests"""
    integration_tests_lib.impl(*args, **kwargs)


@cli.command()
@todo_counter_lib.branch
@todo_counter_lib.count_only_flag
@todo_counter_lib.pattern
def todo_counter(*args, **kwargs):
    """Prints counts of todos and similar comments in the code"""
    todo_counter_lib.impl(*args, **kwargs)


@cli.command()
@pr_validate_lib.tidy
@pr_validate_lib.format
@pr_validate_lib.build
def pr_validate(*args, **kwargs):
    """Runs a set of actions to validate branch state before PR.
    Actions include: building everything, running tests, linter, duck-linter, issue-checker.
    In the future we might add integration tests.
    """
    pr_validate_lib.impl(*args, **kwargs)


@cli.command()
@click.argument("issues", nargs=-1, type=str)
@issue_checker_lib.branch
@issue_checker_lib.no_merge_base
def issue_checker(*args, **kwargs):
    """Checks for occurrences of #issue_number in source files and prints file, line, and summary.

    If no issue numbers are provided, the script will attempt to fetch them from GitHub using the 'gh' CLI.
    You must be authenticated with 'gh' for this to work.
    """
    if not issue_checker_lib.impl(*args, **kwargs):
        exit_with_error(
            "Issue checker found issues numbers related to this pull request in the code"
        )


if __name__ == "__main__":
    if pathlib.Path.cwd() != pathlib.Path(__file__).parent.absolute():
        exit_with_error("Toolbox should be called from the root of the project")

    # Disable traceback for shorter error messages.
    # Comment this line when debugging.
    sys.tracebacklimit = 0

    cli()
