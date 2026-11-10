# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

from click import command, option

from .helpers import (
    all_flag,
    auto_fix,
    branch,
    build_dir,
    no_fix,
    no_merge_base,
)
from ..impl.duckling_formatter import duckling_formatter_impl
from ..impl.helpers import exit_with_error


@command("duckling-formatter")
@all_flag(
    help="Check all Duckling files, not just the ones that are modified",
)
@branch(
    help="The branch relative to which the diff is created.",
)
@no_merge_base()
@build_dir(
    help="Build directory holding bin/duckfmt.",
    prompt=None,
)
@option(
    "--duckfmt",
    "duckfmt_path",
    type=str,
    default=None,
    help="Path to the duckfmt binary. Overrides --build-dir.",
)
@auto_fix()
@no_fix()
def duckling_formatter(*args, **kwargs):
    """Check that Duckling sources (.dk) are formatted with duckfmt.

    By default only the files changed against the branch (plus untracked ones) are
    checked; use --all for the whole repository.
    """
    if not duckling_formatter_impl(*args, **kwargs):
        exit_with_error("Duckling formatting check failed.")
