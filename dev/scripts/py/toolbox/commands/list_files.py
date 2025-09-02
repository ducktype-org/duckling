from click import command, option

from .helpers import (
    all_flag,
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
@all_flag(
    help="List all tracked files instead of just modified files",
)
@branch(
    help="The branch to compare against for modified files (ignored if --all is used).",
)
@no_merge_base(
    help="Compare against the latest commit on branch instead of the merge base. "
         "This feature allows running on a shallow clone.",
)
def list_files(extensions, all, branch, no_merge_base):
    """List files in the repository based on specified criteria.
    
    By default, lists modified files compared to origin/main. Use --all to list all tracked files.
    Use --extensions to filter by file type (e.g., --extensions .cpp --extensions .hpp).
    """
    # Convert extensions tuple to list, or None if empty
    ext_list = list(extensions) if extensions else None
    
    files = list_files_impl(
        extensions=ext_list,
        branch=branch,
        all=all,
        no_merge_base=no_merge_base,
    )
    
    # Output files, one per line
    for file in files:
        print(file)