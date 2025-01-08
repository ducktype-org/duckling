from dataclasses import dataclass
from typing import Optional, Self
import subprocess as sp

from pathlib import Path

from .utils import (
    ExpressionFillError,
    VariableNotFound,
    check_resembles_builtin,
    log_failure,
    log_success,
)

from .config import (
    config_find_and_fill,
    config_find_value,
    CONFIG_KEYS,
    config_get_name_path,
    load_config,
)
from .runcase import RunCase, make_data_from_dict
from ..helpers import (
    BashCommandError,
    clamp_str,
    exit_with_error,
    log_bash,
    log_info,
    log_warning,
    make_singleline_command,
)

TEST_ALLOWED_KEYS = {*CONFIG_KEYS, "RunCases"}
RUN_CASE_ALLOWED_KEYS = {
    "Name",
    "Input",
    "Output",
    "Err",
    "RunArgs",
    "TimeOut",
    "ExitCode",
}


@dataclass
class Test:
    name: str
    description: Optional[str]
    run_cases: list[RunCase]
    cwd: Path

    compile: str
    run: str
    post_run: str
    fail_fast: bool
    clean: str

    def run_test(
        self, test_path: str, dry: bool, fail_fast: bool, verbose: bool
    ) -> bool:
        success = True
        if self.compile:
            self.exec_command(self.compile, cwd=self.cwd, dry=dry)
        for i, run_case in enumerate(self.run_cases):
            if test_path and run_case.name != test_path:
                continue
            log_info(f"Run [{i + 1}/{len(self)}] - {run_case.name}")
            try:
                if err := self.test_case(run_case, dry, verbose):
                    log_failure(
                        f"Case `{self.name}/{run_case.name}` has failed because: {err}"
                    )
                    success = False
                else:
                    if not dry:
                        log_success(f"Case `{self.name}/{run_case.name}` passed")
            except BashCommandError as e:
                log_failure(
                    f"Case `{self.name}/{run_case.name}` has failed because: {''.join(e.args)}"
                )
                success = False
                if fail_fast:
                    break
        return success

    def test_case(self, run_case, dry: bool, verbose: bool) -> bool:
        if verbose:
            var = f"{run_case.input.get_command() + ' | ' if run_case.input else ''} {self.run} {run_case.run_args}"
            var = make_singleline_command(var)
            log_bash(f'cd "{self.cwd.absolute()}" && "{var}"')

        # get input
        test_input = bytes()
        if run_case.input:
            test_input, _ = self.exec_command(
                run_case.input.get_command(), cwd=self.cwd, dry=dry
            )

        # run test
        test_output, test_err = self.exec_command(
            f"{self.run} {run_case.run_args}",
            cwd=self.cwd,
            input=test_input,
            exitcode=run_case.expected_exitcode,
            dry=dry,
        )

        # compare test and expected output
        if run_case.expected_output:
            test_expected_output, _ = self.exec_command(
                run_case.expected_output.get_command(), cwd=self.cwd, dry=dry
            )
            if test_output != test_expected_output:
                return "Stdouts do not match"
                # For now...
                print("", test_output, "\n", test_expected_output)
                exit_with_error(
                    f"Stdouts do not match: {clamp_str(test_output.decode('UTF-8'))} != {clamp_str(test_expected_output.decode('UTF-8'))}"
                )

        # compare test and expected err
        if run_case.expected_err:
            test_expected_err, _ = self.exec_command(
                run_case.expected_err.get_command(), cwd=self.cwd, dry=dry
            )
            if test_err != test_expected_err:
                return "Stderrs do not match"
                # For now...
                print("", test_err, "\n", test_expected_err)
                return False
                # exit_with_error(
                #     f": {clamp_str(test_err.decode('UTF-8'))} != {clamp_str(test_expected_err.decode('UTF-8'))}"
                # )

        # post run
        if self.post_run:
            self.exec_command(
                self.post_run, self.cwd, redirect=False, input=test_output, dry=dry
            )

        return ""

    def exec_command(
        self,
        cmd,
        cwd: Path,
        redirect=True,
        input: bytes | None = None,
        exitcode=0,
        dry: bool = False,
    ):
        if dry:
            cmd = cmd.replace("\n", "\\n")
            log_info(f"[DRY RUN]: cd '{cwd}' && '{cmd}'")
            return bytes(), bytes()
        else:
            proc = sp.Popen(
                ["/bin/bash", "-c", cmd],
                cwd=cwd,
                stdin=sp.PIPE if input else None,
                stdout=sp.PIPE if redirect else None,
                stderr=sp.PIPE if redirect else None,
            )
            stdout, stderr = proc.communicate(input=input)

            status = proc.wait()
            if status != exitcode:
                if stdout is not None:
                    stdout = stdout.decode("UTF-8")
                if stderr is not None:
                    stderr = stderr.decode("UTF-8")
                raise BashCommandError(cmd, status, stdout, stderr, at=cwd)

            return stdout, stderr

    def __str__(self) -> str:
        return f"{self.name}: {self.description if self.description else ''}"

    def __len__(self) -> int:
        return len(self.run_cases)

    def run_clean(self, dry: bool):
        try:
            self.exec_command(self.clean, cwd=self.cwd, redirect=False, dry=dry)
        except BashCommandError:
            log_warning(f"Cleaning has (partially) failed on {self.name}.")


@dataclass
class TestSet:
    name: str
    subtests: list[Self]
    tests: list[Test]

    def test_count(self):
        return len(self.tests) + sum(testset.test_count() for testset in self.subtests)

    def __str__(self) -> str:
        return f"{self.name}: {len(self.tests)} tests"


def load_run_case(test_dict: dict, run_case_name: str) -> RunCase:
    run_case_dict = test_dict["RunCases"][run_case_name]

    for var in run_case_dict:
        check_resembles_builtin(var, RUN_CASE_ALLOWED_KEYS)

    io_data = [None, None, None]
    config_dir = test_dict["_Parent"]["_ConfigFile"].parent
    for i, io in enumerate(["Input", "Output", "Err"]):
        if io in run_case_dict:
            io_data[i] = make_data_from_dict(run_case_dict[io], config_dir)

    return RunCase(
        name=run_case_dict.get("Name", run_case_name),
        run_args=run_case_dict.get("RunArgs", ""),
        input=io_data[0],
        expected_output=io_data[1],
        expected_err=io_data[2],
        expected_exitcode=config_find_value(run_case_dict, "ExitCode", default=0),
        timeout=config_find_value(run_case_dict, "TimeOut", default=1),
    )


def load_test(config: dict, test_name) -> Test:
    test_dict = config["Tests"][test_name]
    test_path = config_get_name_path(test_dict)

    for var in test_dict:
        check_resembles_builtin(var, TEST_ALLOWED_KEYS)

    if not "RunCases" in test_dict or not test_dict["RunCases"]:
        exit_with_error(f"Test {test_path} has no RunCases.")

    try:
        return Test(
            name=test_dict.get("Name", test_name),
            description=test_dict.get("Description"),
            compile=config_find_and_fill(test_dict, "Compile"),
            run=config_find_and_fill(test_dict, "Run"),
            post_run=config_find_and_fill(test_dict, "PostRun"),
            cwd=config["_ConfigFile"].parent,
            run_cases=[
                load_run_case(test_dict, run_case) for run_case in test_dict["RunCases"]
            ],
            clean=config_find_and_fill(test_dict, "Clean"),
            fail_fast=config_find_value(test_dict, "FailFast", default=False),
        )
    except (VariableNotFound, ExpressionFillError) as e:
        exit_with_error(
            f"In test case {test_path}\n\t{e.__class__.__name__}: {''.join(e.args)}"
        )


def load_subtest(config: dict) -> TestSet:
    return TestSet(
        name=config["Name"],
        subtests=[load_subtest(subtest) for subtest in config.get("Subtests", [])],
        tests=[load_test(config, test) for test in config.get("Tests", [])],
    )


def run_tests(
    tests: TestSet, test_path: str, tree, clean, dry, fail_fast, verbose
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
            test.run_clean(dry)
        else:
            log_info(f"Testing: {pth}")
            test_success = test.run_test(
                test_path[len(pth) + 1 :], dry, fail_fast, verbose
            )
            ran_tests += 1
            if not test_success:
                failed_tests.append(pth)
            if fail_fast:
                return failed_tests

    for subtests in tests.subtests:
        new_failed_tests, new_ran_tests = run_tests(
            subtests, test_path, tree.copy(), clean, dry, fail_fast, verbose
        )
        failed_tests += new_failed_tests
        ran_tests += new_ran_tests

    return failed_tests, ran_tests


def integration_tests(clean, dry, test_path, fail_fast, verbose):
    log_info("Running integration tests...")
    config = load_config("tests")
    testset = load_subtest(config)

    failed_tests, ran_tests = run_tests(
        testset, test_path, [], clean, dry, fail_fast, verbose
    )

    if failed_tests:
        num_failed = len(failed_tests)
        failed_tests = map(lambda x: " - " + x, failed_tests)
        exit_with_error(
            f"{'(Fail fast) ' if fail_fast else ''}{num_failed}/{ran_tests} tests failed:\n{'\n'.join(failed_tests)}"
        )
    elif not clean and not dry:
        log_success(f"All [{ran_tests}/{ran_tests}] have run successfully!")
