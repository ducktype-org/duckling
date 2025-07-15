from click import command, option, Choice

from .helpers import build_dir
from ..impl.fix_coverage import fix_coverage_impl


@command()
@build_dir(
    help="The name of the directory"
)
@option(
    "-p",
    "--precision",
    "precision",
    help="Whether to use a surgical approach to clearing stale coverage files."
         "Greater precision takes longer to process, but saves on recompilation time."
         "If it fails, nuking might prove helpful.",
    prompt="Be surgical?",
    default="surgical",
    type=Choice(["surgical", "nuke"]),
)
def fix_coverage(*args, **kwargs):
    """Builds and runs coverage inside given build directory.
    This directory has to have coverage enabled"""
    fix_coverage_impl(*args, **kwargs)
