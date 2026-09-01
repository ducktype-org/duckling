from click import command, option
from .helpers import branch, no_merge_base
from ..impl.header_checker import header_checker_impl
from ..impl.helpers import exit_with_error


@command()
@branch()
@no_merge_base()
@option(
    "-a",
    "--all",
    "all_files",
    is_flag=True,
    default=False,
    help="Check all tracked headers in the repository instead of only modified files",
)
def header_checker(branch: str, no_merge_base: bool, all_files: bool):
    """Validates that .hpp files have #pragma once and .def.hpp files do not"""
    if not header_checker_impl(
        branch=branch, no_merge_base=no_merge_base, all_files=all_files
    ):
        exit_with_error("Header check failed: see the files listed above")
