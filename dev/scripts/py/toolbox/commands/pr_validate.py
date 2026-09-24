from ..impl.pr_validate import pr_validate_impl
from .helpers import (
    auto_fix,
    build_dir,
    clang_format,
    clang_tidy,
    no_fix,
    thread_count,
)
from click import command, option


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
@option(
    "-q",
    "--quiet",
    is_flag=True,
    default=False,
    help="Run the integration test step with `itest --quiet`: report only "
    "failing cases, failing hooks and the summary, instead of every passing "
    "case. The other steps are unaffected.",
)
def pr_validate(*args, **kwargs):
    """Runs a set of actions to validate branch state before PR.
    Actions include: building everything, running tests, integration tests,
    linter, duck-linter, todo-validate, issue-checker.
    """
    pr_validate_impl(*args, **kwargs)
