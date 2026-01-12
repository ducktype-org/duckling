import os
from .helpers import (
    bash_command,
)


def test_impl(
    build_dir,
    memcheck,
    parallel=None,
    label_regex=None,
    tests_regex=None,
    exclude_regex=None,
    verbose=False,
    output_on_failure=False,
    stop_on_failure=False,
    rerun_failed=False,
    quiet=False,
):
    """
    Run tests using ctest with various options.
    
    Args:
        build_dir: Build directory path
        memcheck: Whether to run with valgrind memcheck
        parallel: Number of parallel jobs (None = use all CPUs)
        label_regex: Filter tests by label
        tests_regex: Filter tests by name regex
        exclude_regex: Exclude tests by name regex
        verbose: Enable verbose output
        output_on_failure: Show output only on failure
        stop_on_failure: Stop after first failure
        rerun_failed: Rerun only previously failed tests
        quiet: Quiet mode
    """
    # Build ctest command
    ctest_cmd = "ctest"
    
    # Add parallel execution
    if parallel is not None:
        ctest_cmd += f" -j {parallel}"
    elif not memcheck:
        # Default to parallel execution with all available CPUs when not doing memcheck
        cpu_count = os.cpu_count() or 1
        ctest_cmd += f" -j {cpu_count}"
    
    # Add label filtering
    if label_regex:
        ctest_cmd += f" -L {label_regex}"
    
    # Add test name filtering
    if tests_regex:
        ctest_cmd += f" -R {tests_regex}"
    
    # Add test name exclusion
    if exclude_regex:
        ctest_cmd += f" -E {exclude_regex}"
    
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
    bash_command(f"cd {build_dir} && {ctest_cmd}")
