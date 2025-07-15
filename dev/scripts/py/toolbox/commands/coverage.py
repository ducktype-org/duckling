from ..impl.coverage import coverage_impl
from .helpers import (
    build_dir,
    thread_count,
)
from click import command, option, Choice

@command()
@build_dir(
    help="The name of the directory"
)
@thread_count(
    help="Number of threads used when building",
    default="default",
    type=str,
)
@option(
    "-p",
    "--precision",
    "precision",
    help="Whether to use a surgical approach to clearing stale coverage files."
    "Greater precision saves on recompilation time, but if it fails, nuking might prove helpful.",
    prompt="Be surgical?",
    default="surgical",
    type=Choice(["surgical", "nuke"]),
)
def coverage(*args, **kwargs):
    """Builds and runs coverage inside given build directory.
    This directory has to have coverage enabled"""
    coverage_impl(*args, **kwargs)
