from ..impl.pr_validate import pr_validate_impl
from .helpers import (
    auto_fix,
    build_dir,
    clang_format,
    clang_tidy,
    no_fix,
    thread_count,
)
from click import command


@command()
@build_dir(
    help="Path to build folder with compile_commands.json",
)
@clang_format()
@clang_tidy()
@thread_count(
    help="Number of threads used when building and linting. Defaults to the number of available threads.",
)
@auto_fix()
@no_fix()
def pr_validate(*args, **kwargs):
    """Runs a set of actions to validate branch state before PR.
    Actions include: building everything, running tests, linter, duck-linter, todo-validate, issue-checker.
    In the future we might add integration tests.
    """
    pr_validate_impl(*args, **kwargs)
