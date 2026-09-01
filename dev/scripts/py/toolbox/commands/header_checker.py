import sys
from click import command, option
from ..impl.header_checker import header_checker_impl


@command()
@option(
    "-b",
    "--branch",
    type=str,
    default="origin/main",
    help="Branch to check against (default: origin/main)",
)
@option(
    "--no-merge-base",
    is_flag=True,
    default=False,
    help="Skip merge base calculation when listing files",
)
def header_checker(*args, **kwargs):
    """Validates that .hpp files have #pragma once and .def.hpp files do not"""
    if not header_checker_impl(*args, **kwargs):
        sys.exit(1)
