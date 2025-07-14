from ..impl.duck_linter import duck_linter_impl
from ..impl.helpers import (
    exit_with_error
)
from click import command, option

@command()
@option(
    "-a",
    "--all",
    is_flag=True,
    default=False,
    help="Check all files, not just the ones that are modified",
)
@option(
    "-r",
    "--branch",
    help="The branch relative to which the diff is created.",
    type=str,
    default="origin/main",
)
@option(
    "-v",
    "--verbose",
    is_flag=True,
    default=False,
    help="Also shows checks files that didn't had any errors.",
)
@option(
    "--no-merge-base",
    is_flag=True,
    default=False,
    help="On no-merge-base: compare against the latest commit on `branch` "
    "instead of the commit which is the LCA of `branch` and current branch. "
    "This feature allows to run the linter on a shallow clone.",
)
def duck_linter(*args, **kwargs):
    """Check for violations of
    some of the C++ coding guidelines for Duckling project.
    Current checks:
    * relative import check

    For details see dev-guides.
    """
    passed = duck_linter_impl(*args, **kwargs)
    if not passed:
        exit_with_error("Linting failed.")
