from click import command

from .helpers import (
    build_dir,
    thread_count,
)
from ..impl.coverage import coverage_impl


@command()
@build_dir(help="The name of the directory", default="build-cov")
@thread_count(
    help="Number of threads used when building. Defaults to the number of available threads.",
)
def coverage(*args, **kwargs):
    """Builds and runs coverage inside given build directory.
    This directory has to have coverage enabled"""
    coverage_impl(*args, **kwargs)
