from click import command, option, Choice

from .helpers import build_dir
from ..impl.fix_coverage import fix_coverage_impl


@command()
@build_dir(help="The name of the directory")
@option(
    "-p",
    "--precision",
    "precision",
    help="""Whether to use a surgical approach to clearing stale coverage files.
    Greater precision takes longer to process, but saves on recompilation time.
    If it fails, nuking might prove helpful.""",
    prompt="Be surgical?",
    default="surgical",
    type=Choice(["surgical", "nuke"]),
)
def fix_coverage(*args, **kwargs):
    """Fixes coverage issues by removing stale coverage files inside the given build directory.
    This process helps ensure accurate coverage reporting and may involve surgical or nuke approaches.
    """
    fix_coverage_impl(*args, **kwargs)
