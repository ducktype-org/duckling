from ..impl.test import test_impl
from .helpers import (
    build_dir,
    get_cpu_count,
)
from click import command, option, INT


@command()
@build_dir(
    prompt="build directory with docs enabled",
    help="The name of the build directory with enabled docs.",
)
@option(
    "-m",
    "--memcheck",
    help="Whether or not to perform memcheck with valgrind",
    is_flag=True,
    default=False,
)
@option(
    "-j",
    "--parallel",
    help="Run tests in parallel with optional number of jobs",
    type=INT,
    default=get_cpu_count(),
    show_default=True,
)
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
@option(
    "-V",
    "--verbose",
    help="Enable verbose output from tests",
    is_flag=True,
    default=False,
)
@option(
    "--output-on-failure",
    help="Output anything outputted by the test program if the test should fail",
    is_flag=True,
    default=False,
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
    "-Q",
    "--quiet",
    help="Make ctest quiet",
    is_flag=True,
    default=False,
)
def test(*args, **kwargs):
    """Performs tests of the code"""
    test_impl(*args, **kwargs)
