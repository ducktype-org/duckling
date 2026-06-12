from ..impl.todo_validate import todo_validate_impl
from .helpers import (
    branch,
    no_merge_base,
    all_flag,
)
from ..impl.helpers import (
    exit_with_error,
)
from click import command, option


@command()
@branch(
    help="The branch relative to which the diff is created.",
)
@no_merge_base()
@option(
    "--exclude-files",
    multiple=True,
    type=str,
    default=["todo_validate.py", "todo_counter.py", "CLAUDE.md"],
    help="Files to exclude from checking (e.g., --exclude-files todo_validate.py). "
    "ALL file paths that end with any given value will be excluded."
    "Note that by default [todo_validate.py, todo_counter.py] patterns are excluded.",
)
@all_flag(help="Whether to scan the entire project, not just the diff.")
@option(
    "--print-todos",
    is_flag=True,
    default=False,
    help="Print the list of newly added TODOs with issue numbers (for quacker bot).",
)
def todo_validate(*args, **kwargs):
    """Validates that TODO comments found in the source files follow the required format: @TODO: #issue_number description.
    By default just the diff is checked. If you want to test the entire project use the `--all` flag.
    You can use "!" to escape the verifier, like this: !@TODO: Testing
    """
    if not todo_validate_impl(*args, **kwargs):
        exit_with_error(
            "TODO validation failed: Found TODO/FIXME comments that don't follow the required format"
        )
