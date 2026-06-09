import json
from pathlib import Path

from .test_loader import Case, Test, TestNode, load_tests
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
        determinism_check: bool | None = None,
        custom_values: str = "{}",
):
    """
    The driver function of Duckling Integration Tests framework.
    """
    log_info("Running integration tests...")

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

    user_values = json.loads(custom_values)

    user_values["build_dir"] = str(Path(build_dir).absolute())
    user_values["dev_dir"] = str(get_dev_directory())
    if determinism_check is not None:
        user_values["enable_conc_deterministic_tests"] = str(determinism_check).lower()

    test_set = load_tests("integration_tests", user_values=user_values)

    (succeeded, failed, disabled) = run_tests(
        test_set, filter, [], clean, dry, fail_fast, verbose, log_file
    )

    if dry:
        return

    total_test_count = len(succeeded) + len(failed) + len(disabled)
    print(f"Ran test count: {total_test_count}")
    print(f" - Succeeded: {len(succeeded)}")
    print(f" - Disabled:  {len(disabled)}")
    print(f" - Failed:    {len(failed)}")

    if len(failed):
        failed_tests = map(lambda x: " - " + x, failed)
        exit_with_error(
            f"{'(Fail fast) ' if fail_fast else ''}Failed tests:\n{'\n'.join(failed_tests)}\n"
            + f"Please see log file '{log_file.absolute()}' for more info."
        )
    elif not clean:
        print_success(f"All tests have run successfully!")


def run_test(
        test: Test,
        path: str,
        filter: str,
        dry: bool,
        fail_fast: bool,
        verbose: bool,
        log_file: Path,
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
    simplified_filter = filter[len(path) + 1:]
    log_info(f"===== {path} =====")
    for i, case in enumerate(test.cases):
        if not case.name.startswith(simplified_filter):
            continue
        log_info_if_needed(f"Run [{i + 1}/{len(test)}] - {case.name}", dry, verbose)
        case_path = path + "/" + case.name
        try:
            match run_case(test, case, dry, verbose, log_file):
                case Failure(error):
                    print_failure(f"Case `{case.name}` has failed because: {error}")
                    stats.failed.append(case_path)
                case Success():
                    if not dry:
                        stats.succeeded.append(case_path)
                        print_success(f"Case `{case.name}` passed")
                case Disabled():
                    if not dry:
                        stats.disabled.append(case_path)
                        print_neutral(f"Case `{case.name}` disabled")
        except BashCommandError as e:
            if e.exit_code == 124:
                print_failure(
                    f"Case `{test.name}/{case.name}` has failed with exit code 124 - likely timed out after {case.timeout} second(s)."
                )
            else:
                print_failure(f"Case `{test.name}/{case.name}` has failed.")
            write_log(
                f"{test.name}/{case.name} has failed:\n{''.join(e.args)}\n",
                log_file=log_file,
            )
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
        filter: str,
        tree: list[str],
        clean: bool,
        dry: bool,
        fail_fast: bool,
        verbose: bool,
        log_file: Path,
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

    # Check if the current node is relevant to the filter
    current_path = "/".join(tree)
    if not current_path.startswith(filter) and not filter.startswith(current_path):
        return all_stats

    # Pre-node command
    if node.pre_node:
        log_info_if_needed("Executing pre-node command...", dry, verbose)
        dit_exec_command(
            node.pre_node,
            cwd=node.cwd,
            capture_output=not verbose,
            dry=dry,
            verbose=verbose,
        )

    for test in node.tests:
        path = "/".join(tree + [test.name])

        # This is tricky, as normally it would be enough to check for the prefix
        # like `pth.startswith(test_path)`, but names of test **cases** are not included
        # in the tree list of nodes, but can be in `test_path`, so we have to this both ways.
        if not path.startswith(filter) and not filter.startswith(path):
            continue

        if clean:
            clean_test(test, path, dry, verbose)
        else:
            stats = run_test(test, path, filter, dry, fail_fast, verbose, log_file)
            all_stats += stats
            if fail_fast and len(stats.failed) > 0:
                return all_stats

    for subtest in node.subtests:
        all_stats += run_tests(
            subtest, filter, tree.copy(), clean, dry, fail_fast, verbose, log_file
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
