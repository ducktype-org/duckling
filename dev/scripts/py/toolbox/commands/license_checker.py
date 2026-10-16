# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

from ..impl.license_checker import license_checker_impl
from .helpers import (
    all_flag,
    auto_fix,
    branch,
    no_fix,
    no_merge_base,
)
from ..impl.helpers import exit_with_error
from click import command


@command()
@all_flag(
    help="Check all tracked files, not just the ones that are modified",
)
@branch(
    help="The branch relative to which the diff is created.",
)
@no_merge_base()
@auto_fix()
@no_fix()
def license_checker(*args, **kwargs):
    """Check that every source file (C/C++, Rust, Python) starts with the
    DuckType Compiler License header. Only a shebang line may precede it.

    By default just the diff is checked. If you want to check the entire project
    use the `--all` flag. Missing headers can be added automatically.
    """
    if not license_checker_impl(*args, **kwargs):
        exit_with_error("License header check failed.")
