from ..impl.integration.tester import (
    DEFAULT_LOG_FILE_PATH,
    tester_impl,
)
from .helpers import (
    build_dir,
    get_cpu_count,
    verbose,
)
from click import command, option


@command()
@build_dir(
    help="The name of the project build directory which is passed to the framework."
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
    "--determinism-check/--no-determinism-check",
    is_flag=True,
    default=None,
    help="Enable or disable concurrent deterministic compilation checks for all tests.",
)
@option(
    "-j",
    "--jobs",
    type=int,
    default=get_cpu_count(),
    help="Budget of machine threads the run may use at once. A case occupies its "
    "`NeededThreads` (1 by default) for as long as it runs, so this is an upper "
    "bound on the threads in flight, not on the number of cases. A case needing "
    "more than the budget is rejected; use --sequential to run such tests.",
)
@option(
    "-s",
    "--sequential",
    is_flag=True,
    default=False,
    help="Run one case at a time, ignoring the -j thread budget and every "
    "`NeededThreads`. Output is printed live instead of in per-test sections.",
)
@option(
    "-t",
    "--filter",
    type=str,
    default="",
    help="Run only the test cases whose 'node/.../test/case' path matches the given "
    "regex (searched anywhere in the path). Plain strings work as fuzzy filters: "
    "a path prefix, an inner directory name or a test name.",
)
@option(
    "--deterministic-output",
    is_flag=True,
    default=False,
    help="Print test outputs in the definition (tree) order instead of the "
    "completion order. Has no effect with --sequential.",
)
@option(
    "--core-dumps",
    is_flag=True,
    default=False,
    help="Keep core dumps enabled for test commands. By default every command runs "
    "with `ulimit -c 0`, so a crashing test exits without waiting for the system "
    "core-dump handler.",
)
@option(
    "--timeout-scale",
    type=float,
    default=1.0,
    help="Multiply every resolved TimeOut by this factor, e.g. on slow or heavily "
    "loaded machines.",
)
@option(
    "--custom-values",
    type=str,
    default="{}",
    help="A json dict with configuration values that will override the test defaults. "
    "Note that boolean values should be of a string type, like \"true\" or \"false\"."
)
@verbose(help="Prints some debug information about test cases")
def itest(*args, **kwargs):
    """Runs integration tests"""
    tester_impl(*args, **kwargs)
