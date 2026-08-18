from typing import Dict, List, Tuple, Union
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
    "only_modified",
    is_flag=True,
    help="List only modified files instead of all tracked files",
)
@branch(
    help="The branch to compare against for modified files (ignored if --modified is not used).",
)
@option(
    "--lines",
    is_flag=True,
    help="Return line ranges for each file",
)
@option(
    "--include-untracked",
    is_flag=True,
    help="Also list untracked files (gitignored ones excluded). Without it, untracked "
    "files are only reported as a warning on stderr, as git does not diff them.",
)
@no_merge_base(
    help="Compare against the latest commit on branch instead of the merge base. "
    "This feature allows running on a shallow clone.",
)
def list_files(
    extensions: Tuple[str, ...],
    only_modified: bool,
    lines: bool,
    include_untracked: bool,
    branch: str,
    no_merge_base: bool,
) -> None:
    """List files in the repository based on specified criteria.

    By default, lists all tracked files. Use --modified to list only modified files compared to origin/main.
    Use --extensions to filter by file type (e.g., --extensions .cpp --extensions .hpp).
    Use --lines to get line ranges for each file (full ranges for all files, or specific ranges for modified files).
    Use --include-untracked to list untracked files as well, e.g. to format a brand new file.
    """
    # Convert extensions tuple to list, or None if empty
    ext_list = list(extensions) if extensions else None

    files: Union[List[str], Dict[str, List[Tuple[int, int]]]] = list_files_impl(
        extensions=ext_list,
        branch=branch,
        only_modified=only_modified,
        no_merge_base=no_merge_base,
        lines=lines,
        include_untracked=include_untracked,
    )

    # Output files or files with line ranges
    if lines:
        # When lines flag is used, output is a dict
        for file, line_ranges in files.items():
            for start, end in line_ranges:
                print(f"{file}:{start}-{end}")
    else:
        # Output files, one per line
        for file in files:
            print(file)
