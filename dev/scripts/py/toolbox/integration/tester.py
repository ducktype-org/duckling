from pathlib import Path
from .runcase import RunCase
from .test_loader import Test, TestSet, load_subtest

from .utils import (
    exec_command,
    print_failure,
    print_success,
    write_log,
)

from .config import (
    load_config,
)

from ..helpers import (
    BashCommandError,
    exit_with_error,
    get_input,
    log_bash,
    log_info,
    log_warning,
    make_singleline_command,
    replace_special,
)

DEFAULT_LOG_FILE_PATH = Path("/tmp/toolbox-tester.log")


def run_test(
    test: Test,
    test_path: str,
    dry: bool,
    fail_fast: bool,
    verbose: bool,
    log_file: Path,
) -> bool:
    success = True
    if test.compile:
        exec_command(test.compile, cwd=test.cwd, dry=dry, verbose=verbose)
    for i, run_case in enumerate(test.run_cases):
        if not run_case.name.startswith(test_path):
            continue
        log_info(f"Run [{i + 1}/{len(test)}] - {run_case.name}")
        try:
            if err := test_case(test, run_case, dry, verbose, log_file):
                print_failure(
                    f"Case `{test.name}/{run_case.name}` has failed because: {err}"
                )
                success = False
            else:
                if not dry:
                    print_success(f"Case `{test.name}/{run_case.name}` passed")
        except BashCommandError as e:
            if e.exit_code == 124:
                print_failure(
                    f"Case `{test.name}/{run_case.name}` has timed out after {run_case.timeout} second(s)."
                )
            else:
                print_failure(f"Case `{test.name}/{run_case.name}` has failed.")
            write_log(f"{test.name}/{run_case.name} has failed:\n{''.join(e.args)}\n")
            success = False
            if fail_fast:
                break
    return success


def log_test_out_differs(test, run_case, message, got, expected, log_file):
    write_log(
        (
            f"{test.name}/{run_case.name}: {message}\n"
            f"[GOT]:\n{got.decode('UTF-8')}\n"
            f"[EXPECTED]:\n{expected.decode('UTF-8')}\n"
        ),
        log_file,
    )


def test_case(
    test: Test, run_case: RunCase, dry: bool, verbose: bool, log_file: Path
) -> bool:
    if dry or verbose:
        # This command is supposed to replicate the behavior of running a test case,
        # but this command is not executed directly in order to have more control over
        # what fails and what does not.
        command = (
            f"{run_case.input.get_command() + ' | ' if run_case.input else ''}"
            f"timeout {run_case.timeout}s {test.run} {run_case.run_args}"
        )
        if out := run_case.expected_err:
            command += f" > >(diff <({out.get_command()}) -)"
        if err := run_case.expected_err:
            command += f" 2> >(diff <({err.get_command()}) -)"

        command = replace_special(command)
        command = make_singleline_command(command)
        log_bash(f'cd "{test.cwd.absolute()}" && {command}')
        if test.post_run:
            log_bash(f'cd "{test.cwd.absolute()}" && {replace_special(test.post_run)}')

        if dry:
            return ""

    # get input
    test_input = bytes()
    if run_case.input:
        test_input, _ = exec_command(run_case.input.get_command(), cwd=test.cwd)

    # run test
    test_output, test_err = exec_command(
        f"timeout {run_case.timeout}s {test.run} {run_case.run_args}",
        cwd=test.cwd,
        input=test_input,
        exitcode=run_case.expected_exitcode,
    )

    # compare test and expected output
    if run_case.expected_output:
        test_expected_output, _ = exec_command(
            run_case.expected_output.get_command(), cwd=test.cwd
        )
        if test_output != test_expected_output:
            log_test_out_differs(
                test,
                run_case,
                "Stdouts do not match.",
                test_output,
                test_expected_output,
                log_file,
            )
            return f"Stdouts do not match."

    # compare test and expected err
    if run_case.expected_err:
        test_expected_err, _ = exec_command(
            run_case.expected_err.get_command(), cwd=test.cwd
        )
        if test_err != test_expected_err:
            log_test_out_differs(
                test,
                run_case,
                "Stderrs do not match.",
                test_err,
                test_expected_err,
                log_file,
            )
            return f"Stderrs do not match."

    # post run
    if test.post_run:
        exec_command(test.post_run, test.cwd, redirect=False, input=test_output)

    return ""


def clean_test(test, dry: bool, verbose: bool):
    try:
        if test.clean:
            exec_command(
                test.clean, cwd=test.cwd, redirect=False, dry=dry, verbose=verbose
            )
    except BashCommandError:
        log_warning(f"Cleaning has (partially) failed on {test.name}.")


def run_tests(
    tests: TestSet, test_path: str, tree, clean, dry, fail_fast, verbose, log_file: Path
) -> list[str]:
    tree.append(tests.name)
    failed_tests = []
    ran_tests = 0

    for test in tests.tests:
        pth = "/".join(tree + [test.name])
        if not pth.startswith(test_path) and not test_path.startswith(pth):
            continue
        if clean:
            log_info(f"Cleaning: {pth}")
            clean_test(test, dry, verbose)
        else:
            log_info(f"Testing: {pth}")
            test_success = run_test(
                test, test_path[len(pth) + 1 :], dry, fail_fast, verbose, log_file
            )
            ran_tests += 1
            if not test_success:
                failed_tests.append(pth)
            if fail_fast:
                return failed_tests, ran_tests

    for subtests in tests.subtests:
        new_failed_tests, new_ran_tests = run_tests(
            subtests, test_path, tree.copy(), clean, dry, fail_fast, verbose, log_file
        )
        failed_tests += new_failed_tests
        ran_tests += new_ran_tests

    return failed_tests, ran_tests


def integration_tests(clean, dry, test_path, fail_fast, verbose, log_file):
    log_info("Running integration tests...")

    log_file = Path(log_file)
    if log_file.exists():
        if log_file.absolute() != DEFAULT_LOG_FILE_PATH and log_file.stat().st_size:
            log_warning(f"File: {log_file} already exists and is NOT empty! It will be overwritten!")
            inp = get_input(f"Continue anyway: [y/N] ").lower()
            if inp not in ["y"]:
                exit_with_error("Exiting...")
        log_file.unlink()
        log_file = Path(log_file)

    config = load_config("tests")
    test_set = load_subtest(config)

    failed_tests, ran_tests = run_tests(
        test_set, test_path, [], clean, dry, fail_fast, verbose, log_file
    )

    if failed_tests:
        num_failed = len(failed_tests)
        failed_tests = map(lambda x: " - " + x, failed_tests)
        exit_with_error(
            f"{'(Fail fast) ' if fail_fast else ''}{num_failed}/{ran_tests} tests failed:\n{'\n'.join(failed_tests)}\n"
            + f"Please see log file '{log_file.absolute()}' for more info."
        )
    elif not clean and not dry:
        print_success(f"All [{ran_tests}/{ran_tests}] have run successfully!")
