from ..impl.cpp_linter import cpp_linter_impl
from ..impl.helpers import (
    exit_with_error,
)
from click import command, option
import os

@command()
@option(
    "-t",
    "--tidy",
    "clang_tidy_path",
    prompt="clang-tidy path",
    help="Path to clang-tidy, ex. /usr/bin/clang-tidy-19 or clang-tidy",
    default="clang-tidy-19",
)
@option(
    "-f",
    "--format",
    "clang_format_path",
    prompt="clang-format path",
    help="Path to clang-format, ex. /usr/bin/clang-format-19 or clang-format",
    default="clang-format-19",
)
@option(
    "-b",
    "--build",
    prompt="build folder",
    help="Path to build folder with compile_commands.json",
    default="build",
)
@option(
    "-j",
    "--threads",
    help="On how many threads can linter use. Defaults to os.cpu_count()",
    type=int,
    default=os.cpu_count() or 1,
)
@option(
    "-r",
    "--branch",
    help="The branch relative to which the diff is created.",
    type=str,
    default="origin/main",
)
@option(
    "-a",
    "--all",
    is_flag=True,
    default=False,
    help="Check all files, not just the ones that are modified",
)
@option(
    "--no-merge-base",
    is_flag=True,
    default=False,
    help="On no-merge-base: compare against the latest commit on `branch` "
    "instead of the commit which is the LCA of `branch` and current branch. "
    "This feature allows to run the linter on a shallow clone.",
)
def cpp_linter(*args, **kwargs):
    """Simulates clang-tidy and clang-format as if in a workflow.

    It compares the current branch's working tree with the most recent common ancestor shared with the 'main' branch (called the merge base).
    """
    clang_tidy_failed, clang_format_failed = cpp_linter_impl(*args, **kwargs)
    if clang_tidy_failed or clang_format_failed:
        exit_with_error(
            f"Linter has failed because: {clang_tidy_failed=}, {clang_format_failed=}"
        )