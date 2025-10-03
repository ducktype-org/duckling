from ..impl.todo_validate import todo_validate_impl
from .helpers import (
    branch,
    no_merge_base,
)
from ..impl.helpers import (
    exit_with_error,
)
from click import command, option


@command()
@branch(
    help="The branch relative to which the diff is created.",
)
@no_merge_base(
    help="On no-merge-base: compare against the latest commit on `branch` "
    "instead of the commit which is the LCA of `branch` and current branch. "
    "This feature allows to run the checker on a shallow clone.",
)
@option(
    "--exclude-files",
    multiple=True,
    help="Files to exclude from checking (e.g., --exclude-files todo_validate.py). "
    "All file paths that end with any given value will be excluded.",
)
def todo_validate(*args, **kwargs):
    """Validates that all TODO comments follow the required format: @TODO: #issue_number description.
    You can use "!" to escape the verifier, like this: !@TODO: Testing

    Scans source files for TODO/FIXME comments and ensures they are linked to GitHub issues.
    """
    if not todo_validate_impl(*args, **kwargs):
        exit_with_error(
            "!TODO validation failed: Found !TODO/!FIXME comments that don't follow the required format"
        )
