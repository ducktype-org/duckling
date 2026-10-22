# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

from click import command
from .helpers import all_flag, branch, no_merge_base
from ..impl.header_checker import header_checker_impl
from ..impl.helpers import exit_with_error


@command()
@all_flag(
    help="Whether to scan the entire project, not just the diff.",
)
@branch(
    help="The branch relative to which the diff is checked.",
)
@no_merge_base()
def header_checker(*args, **kwargs):
    """Validates that .hpp files have #pragma once and .def.hpp files do not"""
    if not header_checker_impl(
        *args, **kwargs
    ):
        exit_with_error("Header check failed: see the files listed above")
