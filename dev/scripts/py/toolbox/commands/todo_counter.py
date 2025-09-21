from ..impl.todo_counter import todo_counter_impl
from click import command, option


@command()
@option(
    "-b",
    "--branch",
    type=str,
    default="",
    help="Branch for which to count (empty string means the current branch). It can also be any other commit reference understood be git (e.g. HEAD~1)",
)
@option(
    "-c",
    "--count-only",
    is_flag=True,
    default=False,
    help="Only show the counts and nothing else",
)
@option(
    "-p",
    "--pattern",
    type=str,
    required=False,
    multiple=True,
    help="Search for a given pattern instead of the default ones (todo and fixme). If passed multiple times, all the patterns will be searched for",
)
def todo_counter(*args, **kwargs):
    """Prints counts of todos and similar comments in the code"""
    todo_counter_impl(*args, **kwargs)
