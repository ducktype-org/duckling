from ..impl.docs import docs_impl
from .helpers import (
    build_dir,
)
from click import command


@command()
@build_dir(
    prompt="build directory with docs enabled",
    help="The name of the build directory with enabled docs.",
)
def docs(*args, **kwargs):
    """Builds a documentation for the project and opens it in the browser"""
    docs_impl(*args, **kwargs)
