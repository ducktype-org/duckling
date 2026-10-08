# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

from click import command

from .helpers import (
    all_flag,
    auto_fix,
    branch,
    build_dir,
    clang_format,
    clang_tidy,
    no_fix,
    no_merge_base,
    thread_count,
)
from ..impl.cpp_linter import cpp_linter_impl
from ..impl.helpers import (
    exit_with_error,
)


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
@clang_format()
@clang_tidy()
@no_merge_base()
@thread_count(
    help="Number of threads used when linting. Defaults to the number of available threads.",
)
@auto_fix()
@no_fix()
def cpp_linter(*args, **kwargs):
    """Simulates clang-tidy and clang-format as if in a workflow.

    It compares the current branch's working tree with the most recent common ancestor shared with the 'main' branch (called the merge base).
    """
    clang_tidy_failed, clang_format_failed = cpp_linter_impl(*args, **kwargs)
    if clang_tidy_failed or clang_format_failed:
        exit_with_error(
            f"Linter has failed because: {clang_tidy_failed=}, {clang_format_failed=}"
        )
