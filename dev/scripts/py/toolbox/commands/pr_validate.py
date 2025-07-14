from ..impl.pr_validate import pr_validate_impl
from .helpers import (
    build_dir,
    format,
    tidy,
)
from click import command

@command()
@format
@tidy
@build_dir(
    prompt="build folder",
    help="Path to build folder with compile_commands.json",
)
def pr_validate(*args, **kwargs):
    """Runs a set of actions to validate branch state before PR.
    Actions include: building everything, running tests, linter, duck-linter, issue-checker.
    In the future we might add integration tests.
    """
    pr_validate_impl(*args, **kwargs)