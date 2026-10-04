from ..impl.duck_linter import duck_linter_impl
from .helpers import (
    all_flag,
    auto_fix,
    branch,
    no_fix,
    no_merge_base,
    verbose,
)
from ..impl.helpers import exit_with_error
from click import command, option


@command()
@all_flag(
    help="Check all files, not just the ones that are modified",
)
@branch(
    help="The branch relative to which the diff is created.",
)
@no_merge_base()
@auto_fix()
@no_fix()
@verbose(help="Also shows checked files that didn't have any errors.")
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
