from ..impl.coverage import coverage_impl
from .helpers import (
    build_dir,
    thread_count,
)
from click import command

@command()
@build_dir(prompt="Build dir name", help = "The name of the directory")
@thread_count(
    help="Number of threads used when building",
)
def coverage(*args, **kwargs):
    """Builds and runs coverage inside given build directory.
    This directory has to have coverage enabled"""
    coverage_impl(*args, **kwargs)
