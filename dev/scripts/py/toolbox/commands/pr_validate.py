from ..impl.pr_validate import pr_validate_impl
from click import command, option

@command()
@option(
    "-t",
    "--tidy",
    "clang_tidy_path",
    prompt="clang-tidy path",
    help="Path to clang-tidy, ex. /usr/bin/clang-tidy-19 or clang-tidy",
    default="clang-tidy-19",
)
@option(
    "-f",
    "--format",
    "clang_format_path",
    prompt="clang-format path",
    help="Path to clang-format, ex. /usr/bin/clang-format-19 or clang-format",
    default="clang-format-19",
)
@option(
    "-b",
    "--build",
    prompt="build folder",
    help="Path to build folder with compile_commands.json",
    default="build",
)
def pr_validate(*args, **kwargs):
    """Runs a set of actions to validate branch state before PR.
    Actions include: building everything, running tests, linter, duck-linter, issue-checker.
    In the future we might add integration tests.
    """
    pr_validate_impl(*args, **kwargs)