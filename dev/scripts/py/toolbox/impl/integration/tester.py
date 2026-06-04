from pathlib import Path

import time
from .test_loader import Case, Test, TestNode, load_tests
from .tui_reporter import IntegrationTuiReporter
from .utils import (
    dit_exec_command,
    log_info_if_needed,
    print_failure,
    print_success,
    write_log,
    print_neutral,
    TestStatistics,
    Success,
    Failure,
    Disabled,
)
from ..helpers import (
    BashCommandError,
    exit_with_error,
    get_input,
    log_info,
    log_warning,
    get_dev_directory,
)

DEFAULT_LOG_FILE_PATH = Path("/tmp/dit.log")


def tester_impl(
    clean: bool,
    dry: bool,
    filter: str,
    fail_fast: bool,
    verbose: bool,
    log_file: str | Path,
    build_dir: str,
    tui: bool = False,
    rerun_failed: bool = False,
):
        # Debug print for filter_list after it is set
        # (must be after rerun_failed/filter logic)
    """
    The driver function of Duckling Integration Tests framework.
    """
    log_info("Running integration tests...")

    start_time = time.time()
    log_file = Path(log_file)
    if log_file.exists():
        if log_file.absolute() != DEFAULT_LOG_FILE_PATH and log_file.stat().st_size > 0:
            log_warning(
                f"File: {log_file} already exists and is NOT empty! It will be overwritten!"
            )
            inp = get_input(f"Continue anyway: [y/N] ").lower()
            if inp not in ["y"]:
                exit_with_error("Exiting...")
        log_file.unlink()
        log_file = Path(log_file)

    user_values = {
        "build_dir": str(Path(build_dir).absolute()),
        "dev_dir": str(get_dev_directory()),
    }


    test_set = load_tests("integration_tests", user_values=user_values)

    # Handle rerun-failed logic
    last_failed_file = Path("integration_tests/.last_failed")
    filter_list = None
    def norm(path):
        # Always relative to integration_tests/
        return path[path.find("integration_tests/") + len("integration_tests/"):] if "integration_tests/" in path else path

    if rerun_failed:
        if last_failed_file.exists():
            with open(last_failed_file, "r") as f:
                filter_list = [norm(line.strip()) for line in f if line.strip()]
            if not filter_list:
                print("[itest] No failed tests recorded from previous run. Running all tests.")
                filter_list = None
        else:
            print("[itest] No .last_failed file found. Running all tests.")
    elif filter:
        filter_list = [norm(f) for f in filter.split(",") if f.strip()]

    reporter = IntegrationTuiReporter() if tui else None
    if reporter:
        reporter.on_status("Initializing integration test run")

    (succeeded, failed, disabled) = run_tests(
        test_set, filter_list, [], clean, dry, fail_fast, verbose, log_file, reporter
    )
    if reporter:
        reporter.finish()
        # Print statistics summary at the end (TUI only)
        elapsed = time.time() - start_time
        total_cases = len(succeeded) + len(failed) + len(disabled)
        print("\n==================== Integration Test Summary ====================")
        print(f"Total test cases run: {total_cases}")
        print(f"  Passed:   {len(succeeded)}")
        print(f"  Failed:   {len(failed)}")
        print(f"  Disabled: {len(disabled)}")
        # Unique test nodes (by path up to last /)
        def node_name(path):
            return path.rsplit("/", 1)[0] if "/" in path else path
        all_cases = succeeded + failed + disabled
        unique_nodes = set(node_name(p) for p in all_cases)
        print(f"Unique test nodes run: {len(unique_nodes)}")
        print(f"Total elapsed time: {elapsed:.2f} seconds")
        # List all failed test cases
        if failed:
            print("\nFailed test cases:")
            for case in failed:
                print(f"  - {case}")
        print("================================================================\n")
    if dry:
        return

    # Save failed test node paths for rerun (strip to test node, not case)
    if not clean:
        failed_test_nodes = set()
        for path in failed:
            # path is like 'integration_tests/foo/bar/case', keep 'integration_tests/foo/bar'
            parts = path.split("/")
            if len(parts) > 1:
                failed_test_nodes.add("/".join(parts[:-1]))
        with open(last_failed_file, "w") as f:
            for tpath in sorted(failed_test_nodes):
                f.write(tpath + "\n")

    if len(failed):
        exit_with_error(
            f"{'(Fail fast) ' if fail_fast else ''}Please see log file '{log_file.absolute()}' for more info."
        )


def run_test(
    test: Test,
    path: str,
    filter_list,
    dry: bool,
    fail_fast: bool,
    verbose: bool,
    log_file: Path,
    reporter: IntegrationTuiReporter | None = None,
) -> TestStatistics:
    """
    Runs a test from `Test` object.
    """
    if test.pre_test:
        log_info_if_needed("Executing pre-test...", dry, verbose)
        dit_exec_command(
            test.pre_test,
            cwd=test.cwd,
            capture_output=not verbose,
            dry=dry,
            verbose=verbose,
        )
    stats = TestStatistics([], [], [])
    if reporter:
        reporter.on_test_start(path)

    for i, case in enumerate(test.cases):
        case_path = path + "/" + case.name
        # If filter_list is set, only run cases that match any filter prefix
        if filter_list is not None:
            if not any(case_path.startswith(f) or f.startswith(case_path) for f in filter_list):
                continue
        log_info_if_needed(f"Run [{i + 1}/{len(test)}] - {case.name}", dry, verbose)
        try:
            match run_case(test, case, dry, verbose, log_file):
                case Failure(error):
                    if reporter:
                        reporter.on_case_failed(path, case.name, error)
                    stats.failed.append(case_path)
                case Success():
                    if reporter:
                        reporter.on_case_passed(path, case.name)
                    if not dry:
                        stats.succeeded.append(case_path)
                case Disabled():
                    if reporter:
                        reporter.on_case_disabled(path, case.name)
                    if not dry:
                        stats.disabled.append(case_path)
        except BashCommandError as e:
            write_log(
                f"{test.name}/{case.name} has failed:\n{''.join(e.args)}\n",
                log_file=log_file,
            )
            if reporter:
                error_msg = f"Exit code {e.exit_code}" if e.exit_code == 124 else "Command error"
                reporter.on_case_failed(path, case.name, error_msg)
            stats.failed.append(case_path)
            if fail_fast:
                break
    if test.post_test:
        log_info_if_needed("Executing post-test...", dry, verbose)
        dit_exec_command(
            test.post_test,
            cwd=test.cwd,
            capture_output=not verbose,
            dry=dry,
            verbose=verbose,
        )
    return stats


def log_test_out_differs(test, case, message, got, expected, log_file, verbose):
    """
    Used to dump program's incorrect output.
    """
    to_dump = (
        f"{test.name}/{case.name}: {message}\n"
        f"[GOT]:\n{got.decode('UTF-8')}\n"
        f"[EXPECTED]:\n{expected.decode('UTF-8')}\n"
    )
    log_info_if_needed(to_dump, False, verbose=verbose)
    write_log(
        to_dump,
        log_file,
    )


def run_case(
        test: Test, case: Case, dry: bool, verbose: bool, log_file: Path
) -> Success | Failure | Disabled:
    """
    Runs a test case from `Case` object.
    Returns an empty string on success, and an error message on error.
    """
    # Check if `Enabled` evaluates to `true` to see if test-case is enabled or not
    if case.enabled != "":
        log_info_if_needed("Checking if test-case is enabled...", dry, verbose)
        try:
            dit_exec_command(
                case.enabled,
                cwd=test.cwd,
                capture_output=not verbose,
                dry=dry,
                verbose=verbose,
                exitcode=0,  # expects 0 -- 0 means it has evaluated to `true`
            )
        except BashCommandError:
            return Disabled()

    # Pre-case command
    if case.pre_case:
        log_info_if_needed("Executing pre-case command...", dry, verbose)
        dit_exec_command(
            case.pre_case,
            cwd=test.cwd,
            capture_output=not verbose,
            dry=dry,
            verbose=verbose,
        )

    # Get input.
    test_input = bytes()
    if case.input:
        log_info_if_needed("Getting input", dry, verbose)
        test_input, _ = dit_exec_command(
            case.input.get_command(), cwd=test.cwd, dry=dry, verbose=verbose
        )

    # Run test.
    log_info_if_needed("Running the test case...", dry, verbose)
    test_output, test_err = dit_exec_command(
        f"timeout {case.timeout}s bash -c \'{case.run}\'",
        cwd=test.cwd,
        input=test_input,
        exitcode=case.expected_exitcode,
        dry=dry,
        verbose=verbose,
    )

    # Compare test and expected output.
    if case.expected_output:
        log_info_if_needed("Getting the expected output...", dry, verbose)
        test_expected_output, _ = dit_exec_command(
            case.expected_output.get_command(), cwd=test.cwd, verbose=verbose, dry=dry
        )
        if not dry and test_output != test_expected_output:
            log_test_out_differs(
                test,
                case,
                "Stdouts do not match.",
                test_output,
                test_expected_output,
                log_file,
                verbose,
            )
            return Failure(f"Stdouts do not match.")

    # Compare test and expected err.
    if case.expected_err:
        log_info_if_needed("Getting the expected err...", dry, verbose)
        test_expected_err, _ = dit_exec_command(
            case.expected_err.get_command(), cwd=test.cwd, verbose=verbose, dry=dry
        )
        if not dry and test_err != test_expected_err:
            log_test_out_differs(
                test,
                case,
                "Stderrs do not match.",
                test_err,
                test_expected_err,
                log_file,
                verbose,
            )
            return Failure(f"Stderrs do not match.")

    # Post-case command
    if case.post_case:
        log_info_if_needed("Executing post-case command...", dry, verbose)
        dit_exec_command(
            case.post_case,
            test.cwd,
            capture_output=not verbose,
            input=test_output,
            verbose=verbose,
            dry=dry,
        )

    return Success()


def clean_test(test: Test, path: str, dry: bool, verbose: bool):
    """
    Performs cleaning on a test.
    """
    log_info(f"Cleaning: {path}")
    try:
        if test.clean:
            dit_exec_command(
                test.clean,
                cwd=test.cwd,
                capture_output=False,
                dry=dry,
                verbose=verbose,
            )
    except BashCommandError:
        log_warning(f"Cleaning has (partially) failed on {test.name}.")


def run_tests(
    node: TestNode,
    filter_list,
    tree: list[str],
    clean: bool,
    dry: bool,
    fail_fast: bool,
    verbose: bool,
    log_file: Path,
    reporter: IntegrationTuiReporter | None = None,
    filter_set: set[str] | None = None,
) -> TestStatistics:
    """
    A recursive function for running all tests.
    A single call executes all tests in a given tree node.

    Each tree node is a `testconfig.yaml` file.
    `tree` variable is used to store the names of the nodes on the
    path to the current node in the tree.

    """
    tree.append(node.name)
    all_stats = TestStatistics([], [], [])

    # Check if the current node is relevant to the filter or filter_set
    def norm(path):
        if path == "integration_tests":
            return ""
        return path[path.find("integration_tests/") + len("integration_tests/"):] if "integration_tests/" in path else path
    current_path = norm("/".join(tree))
    if filter_list is not None:
        # Never skip the root node (empty string), only filter non-root nodes
        if current_path != "":
            if not any(current_path.startswith(f) or f.startswith(current_path) for f in filter_list):
                return all_stats

    if reporter:
        reporter.on_node_start(current_path)

    # Pre-node command
    if node.pre_node:
        if reporter:
            reporter.on_status(f"Running pre-node hook for {current_path}")
        log_info_if_needed("Executing pre-node command...", dry, verbose)
        dit_exec_command(
            node.pre_node,
            cwd=node.cwd,
            capture_output=not verbose,
            dry=dry,
            verbose=verbose,
        )

    for test in node.tests:
        path = norm("/".join(tree + [test.name]))

        if filter_list is not None:
            if not any(path.startswith(f) or f.startswith(path) for f in filter_list):
                continue

        if clean:
            clean_test(test, path, dry, verbose)
        else:
            stats = run_test(test, path, filter_list, dry, fail_fast, verbose, log_file, reporter)
            all_stats += stats
            if fail_fast and len(stats.failed) > 0:
                return all_stats

    for subtest in node.subtests:
        all_stats += run_tests(
            subtest, filter_list, tree.copy(), clean, dry, fail_fast, verbose, log_file, reporter
        )

    # Post-node command
    if node.post_node:
        log_info_if_needed("Executing post-node command...", dry, verbose)
        dit_exec_command(
            node.post_node,
            cwd=node.cwd,
            capture_output=not verbose,
            dry=dry,
            verbose=verbose,
        )

    return all_stats
