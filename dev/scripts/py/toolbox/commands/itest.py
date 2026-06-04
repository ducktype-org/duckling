from ..impl.integration.tester import (
    DEFAULT_LOG_FILE_PATH,
    tester_impl,
)
from .helpers import (
    build_dir,
    verbose,
)
from click import command, option


@command()
@build_dir(
    help="The name of the project build directory which is passed to the framework.",
    prompt=False,
)
@option(
    "-c",
    "--clean",
    is_flag=True,
    default=False,
    help="Runs `Clean` command on every test. If passed, no tests are ran.",
)
@option(
    "-d",
    "--dry",
    is_flag=True,
    default=False,
    help="Only prints commands to be executed instead of really executing them",
)
@option(
    "-f",
    "--fail-fast",
    is_flag=True,
    default=False,
    help="Whether to fail upon a testcase failure. If not passed, runs all tests regardless of their result.",
)
@option(
    "-l",
    "--log-file",
    type=str,
    default=str(DEFAULT_LOG_FILE_PATH),
    help="Path to a log file. A log file contains e.g. dumps of program incorrect IO",
)
@option(
    "-t",
    "--filter",
    type=str,
    default="",
    help="Run tests under the specified path prefix (e.g., 'tests/C++' or 'tests/C++/Case1').",
)
@option(
    "--rerun-failed",
    is_flag=True,
    default=False,
    help="Rerun only the tests that failed in the previous run.",
)
@option(
    "--tui",
    is_flag=True,
    default=False,
    help="Use terminal UI for real-time test progress visualization.",
)
@verbose(help="Prints some debug information about test cases")
def itest(*args, **kwargs):
    """Runs integration tests"""
    tester_impl(*args, **kwargs)
