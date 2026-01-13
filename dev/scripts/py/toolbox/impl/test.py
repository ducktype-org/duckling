import os
from .helpers import (
    bash_command,
)


def test_impl(
    build_dir,
    memcheck,
    parallel,
    label_regex=None,
    tests_regex=None,
    exclude_regex=None,
    verbose=False,
    output_on_failure=False,
    stop_on_failure=False,
    rerun_failed=False,
    quiet=False,
):
    # Build ctest command
    ctest_cmd = "ctest"
    
    # Add parallel execution
    ctest_cmd += f" -j {int(parallel)}"
    
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
