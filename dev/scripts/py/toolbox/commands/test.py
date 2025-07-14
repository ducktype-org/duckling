from ..impl.test import test_impl
from click import command, option, BOOL

@command()
@option(
    "-b",
    "--build_dir",
    prompt="build directory with docs enabled",
    help="The name of the build directory with enabled docs.",
    default="build",
)
@option(
    "-m",
    "--memcheck",
    prompt="Memcheck",
    help="Whether or not to perform memcheck with valgrind",
    type=BOOL,
    default=False,
    show_default=True,
)
def test(*args, **kwargs):
    """Performs tests of the code"""
    test_impl(*args, **kwargs)
