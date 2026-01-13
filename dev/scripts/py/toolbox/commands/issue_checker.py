from ..impl.issue_checker import issue_checker_impl
from .helpers import (
    branch,
    no_merge_base,
)
from ..impl.helpers import (
    exit_with_error,
)
from click import argument, command


@command()
@argument("issues", nargs=-1, type=str)
@branch(
    help="The branch relative to which the diff is created.",
)
@no_merge_base(
    help="On no-merge-base: compare against the latest commit on `branch` "
    "instead of the commit which is the LCA of `branch` and current branch. "
    "This feature allows to run the checker on a shallow clone.",
)
def issue_checker(*args, **kwargs):
    """Checks for occurrences of #issue_number in source files and prints file, line, and summary.

    If no issue numbers are provided, the script will attempt to fetch them from GitHub using the 'gh' CLI.
    You must be authenticated with 'gh' for this to work.
    """
    if not issue_checker_impl(*args, **kwargs):
        exit_with_error(
            "Issue checker found issues numbers related to this pull request in the code"
        )
