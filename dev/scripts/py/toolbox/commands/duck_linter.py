from ..impl.duck_linter import duck_linter_impl
from .helpers import (
    all_flag,
    branch,
    verbose,
    no_merge_base,
)
from ..impl.helpers import (
    exit_with_error
)
from click import command

@command()
@all_flag
@branch
@verbose(help="Also shows checked files that didn't had any errors.")
@no_merge_base
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
