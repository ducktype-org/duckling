from typing import Tuple
from click import command, option

from .helpers import (
    branch,
    no_merge_base,
)
from ..impl.list_files import list_files_impl


@command("list-files")
@option(
    "--extensions",
    multiple=True,
    help="File extensions to include (e.g., --extensions .cpp --extensions .hpp). "
         "If not specified, all files are included.",
)
@option(
    "--modified",
    is_flag=True,
    help="List only modified files instead of all tracked files",
)
@branch(
    help="The branch to compare against for modified files (ignored if --modified is not used).",
)
@option(
    "--lines",
    is_flag=True,
    help="Return line ranges for each file (only applicable with --modified)",
)
@no_merge_base(
    help="Compare against the latest commit on branch instead of the merge base. "
         "This feature allows running on a shallow clone.",
)
def list_files(extensions: Tuple[str, ...], modified: bool, lines: bool, branch: str, no_merge_base: bool) -> None:
    """List files in the repository based on specified criteria.
    
    By default, lists all tracked files. Use --modified to list only modified files compared to origin/main.
    Use --extensions to filter by file type (e.g., --extensions .cpp --extensions .hpp).
    Use --lines with --modified to get line ranges for each modified file.
    """
    # Convert extensions tuple to list, or None if empty
    ext_list = list(extensions) if extensions else None
    
    files = list_files_impl(
        extensions=ext_list,
        branch=branch,
        modified=modified,
        no_merge_base=no_merge_base,
        lines=lines,
    )
    
    # Output files or files with line ranges
    if lines and modified:
        # When lines flag is used with modified, output is a dict
        for file, line_ranges in files.items():
            for start, end in line_ranges:
                print(f"{file}:{start}-{end}")
    else:
        # Output files, one per line
        for file in files:
            print(file)