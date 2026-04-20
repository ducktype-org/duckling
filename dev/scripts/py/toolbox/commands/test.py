from ..impl.test import test_impl
from .helpers import (
    build_dir,
    get_cpu_count,
    thread_count,
    verbose,
)
from click import command, option, INT


@command()
@build_dir(
    prompt="build directory",
    help="The name of the build directory.",
)
@option(
    "-m",
    "--memcheck",
    help="Whether or not to perform memcheck with valgrind",
    is_flag=True,
    default=False,
)
@thread_count()
@option(
    "-L",
    "--label-regex",
    help="Run tests with labels matching regular expression (e.g., 'base', 'common', 'compiler')",
    type=str,
    default=None,
)
@option(
    "-R",
    "--tests-regex",
    help="Run tests matching regular expression",
    type=str,
    default=None,
)
@option(
    "-E",
    "--exclude-regex",
    help="Exclude tests matching regular expression",
    type=str,
    default=None,
)
@verbose()
@option(
    "--output-on-failure/--no-output-on-failure",
    help="Output anything outputted by the test program if the test should fail",
    default=True,
)
@option(
    "--stop-on-failure",
    help="Stop running the tests after one has failed",
    is_flag=True,
    default=False,
)
@option(
    "--rerun-failed",
    help="Run only the tests that failed previously",
    is_flag=True,
    default=False,
)
@option(
    "--timeout",
    help="Set a timeout for each test in seconds",
    type=int,
    default=None,
)
@option(
    "-Q",
    "--quiet",
    help="Make ctest quiet",
    is_flag=True,
    default=False,
)
def test(*args, **kwargs):
    """
    Builds and performs tests of the code.
    To run e.g. only VM's tests use `./toolbox.py test -L vm`.
    For more details about each option see ctest documentation.
    """
    test_impl(*args, **kwargs)
