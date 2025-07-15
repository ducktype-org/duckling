from ..impl.cpp_linter import cpp_linter_impl
from .helpers import (
    all_flag,
    branch,
    build_dir,
    clang_format,
    clang_tidy,
    no_merge_base,
    thread_count,
)
from ..impl.helpers import (
    exit_with_error,
)
from click import command
import os

@command()
@all_flag(
    help="Check all files, not just the ones that are modified",
)
@branch(
    help="The branch relative to which the diff is created.",
)
@build_dir(
    help="Path to build folder with compile_commands.json",
)
@clang_format(
    help="Path to clang-format, ex. /usr/bin/clang-format-19 or clang-format",
)
@clang_tidy(
    help="Path to clang-tidy, ex. /usr/bin/clang-tidy-19 or clang-tidy",
)
@no_merge_base(
    help="On no-merge-base: compare against the latest commit on `branch` "
        "instead of the commit which is the LCA of `branch` and current branch. "
        "This feature allows to run the checker on a shallow clone.",
)
@thread_count(
    help="On how many threads can linter use. Defaults to os.cpu_count()",
    default=os.cpu_count() or 1,
    type=int
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