from ..impl.coverage import coverage_impl
from click import command, option

@command()
@option(
    "-b",
    "--build_dir",
    prompt="Build directory",
    help="The name of the directory.",
    default="build",
)
@option(
    "-j",
    "--thread-count",
    prompt="Number of threads used when building",
    help="Number of threads used when building",
    type=str,
    default="default",
)
def coverage(*args, **kwargs):
    """Builds and runs coverage inside given build directory.
    This directory has to have coverage enabled"""
    coverage_impl(*args, **kwargs)
