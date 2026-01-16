import os
import re
from .helpers import (
    bash_command,
    bash_command_get_output,
)


def get_available_test_targets(build_dir):
    """
    Query the build system for available build_*_tests targets.
    Returns a list of test pack names (e.g., ['base', 'common', 'compiler', 'vm'])
    """
    try:
        # Try to get targets from Ninja
        stdout, _ = bash_command_get_output(f"ninja -C {build_dir} -t targets all 2>/dev/null || true")
        if stdout:
            # Parse ninja output for build_*_tests targets
            targets = []
            for line in stdout.split('\n'):
                match = re.match(r'^build_(\w+)_tests:', line)
                if match:
                    targets.append(match.group(1))
            if targets:
                return targets
    except:
        pass
    
    # Fallback: return empty list, which will cause build_all_tests to be used
    return []


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
    # Determine which tests to build
    build_target = "build_all_tests"
    
    if label_regex:
        # Get available test targets from build directory
        available_targets = get_available_test_targets(build_dir)
        
        # Check if label_regex is a simple identifier (not a regex)
        # and matches exactly one of the available test packs
        if available_targets and re.match(r'^\w+$', label_regex) and label_regex in available_targets:
            # Build only the specific test pack
            build_target = f"build_{label_regex}_tests"
    
    # Build tests
    bash_command(f"cmake --build {build_dir} --target {build_target} -j {int(parallel)}")
    
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
