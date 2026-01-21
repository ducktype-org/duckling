import re
from .helpers import (
    BashCommandError,
    bash_command,
    bash_command_get_output,
    exit_with_error,
)


def get_available_test_targets(build_dir: str) -> list[str]:
    """
    Query the build system for available build_*_tests targets.
    Returns a list of test pack names (e.g., ['all', 'base', 'common', 'compiler', 'vm'])
    """
    targets: list[str] = []

    try:
        # Get list of targets from CMake File API.
        # This should work for any build-system, but is a little technical.
        stdout, _ = bash_command_get_output("ls .cmake/api/v1/reply", cwd=build_dir)
        for line in stdout.split("\n"):
            match = re.search(r"\bbuild_(\w+)_tests\b", line)
            if match:
                targets.append(match.group(1))
        return targets
    except BashCommandError as e:
        exit_with_error(
            "Error querying CMake File API. Please re-run `./toolbox.py setup-build`."
        )


def test_impl(
    build_dir: str,
    memcheck: bool,
    parallel: int,
    label_regex: str | None = None,
    tests_regex: str | None = None,
    exclude_regex: str | None = None,
    verbose: bool = False,
    output_on_failure: bool = False,
    stop_on_failure: bool = False,
    rerun_failed: bool = False,
    quiet: bool = False,
):
    # Determine which tests to build
    build_targets = []

    if label_regex:
        # Get available test targets from build directory
        available_targets = get_available_test_targets(build_dir)

        if available_targets:
            # Try to match the label_regex against each available target
            pattern = re.compile(label_regex)
            matching_targets = [
                target for target in available_targets if pattern.match(target)
            ]

            if matching_targets:
                # Build only the matching test packs
                build_targets = [f"build_{target}_tests" for target in matching_targets]

    # If no specific targets identified, build all tests
    if not build_targets:
        build_targets = ["build_all_tests"]

    # Build tests (can be multiple targets)
    for target in build_targets:
        bash_command(f"cmake --build {build_dir} --target {target} -j {int(parallel)}")

    # Build ctest command
    ctest_cmd = "ctest"

    # Add parallel execution
    ctest_cmd += f" -j {int(parallel)}"

    # Add label filtering
    if label_regex:
        ctest_cmd += f' -L "{label_regex}"'

    # Add test name filtering
    if tests_regex:
        ctest_cmd += f' -R "{tests_regex}"'

    # Add test name exclusion
    if exclude_regex:
        ctest_cmd += f' -E "{exclude_regex}"'

    # Add verbose output
    if verbose:
        ctest_cmd += " -V"

    # Add output on failure
    if output_on_failure:
        ctest_cmd += " --output-on-failure"

    # Add stop on failure
    if stop_on_failure:
        ctest_cmd += " --stop-on-failure"

    # Add rerun failed
    if rerun_failed:
        ctest_cmd += " --rerun-failed"

    # Add quiet mode
    if quiet:
        ctest_cmd += " -Q"

    # Add memcheck if requested
    if memcheck:
        ctest_cmd += " --force-new-ctest-process --test-action memcheck"

    # Execute in build directory
    bash_command(ctest_cmd, cwd=build_dir)
