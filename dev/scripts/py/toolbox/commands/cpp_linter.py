from ..impl.cpp_linter import cpp_linter_impl
from .helpers import (
    tidy,
    format,
    branch,
    all_flag,
    no_merge_base,
    build_dir,
    thread_count,
)
from ..impl.helpers import (
    exit_with_error,
)
from click import command
import os

@command()
@tidy
@format
@build_dir(
    prompt="build folder",
    help="Path to build folder with compile_commands.json",
)
@thread_count(
    help="On how many threads can linter use. Defaults to os.cpu_count()",
)
@branch
@all_flag
@no_merge_base
def cpp_linter(*args, **kwargs):
    """Simulates clang-tidy and clang-format as if in a workflow.

    It compares the current branch's working tree with the most recent common ancestor shared with the 'main' branch (called the merge base).
    """
    clang_tidy_failed, clang_format_failed = cpp_linter_impl(*args, **kwargs)
    if clang_tidy_failed or clang_format_failed:
        exit_with_error(
            f"Linter has failed because: {clang_tidy_failed=}, {clang_format_failed=}"
        )