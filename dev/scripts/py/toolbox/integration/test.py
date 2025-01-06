from dataclasses import dataclass
from typing import Optional, Self
import subprocess as sp

from pathlib import Path

from .utils import (
    ExpressionFillError,
    VariableNotFound,
    check_resembles_builtin,
    make_test_name,
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
    timeout: int
    fail_fast: bool
    clean: str

    def run_test(self):
        if self.compile:
            self.exec_command(self.compile, cwd=self.cwd)
        for i, run_case in enumerate(self.run_cases):
            log_info(f" -- [{i + 1}/{len(self)}] - {run_case.name}")

            var = f"{run_case.input.get_command() + ' | ' if run_case.input else ''} {self.run} {run_case.run_args}"
            var = make_singleline_command(var)

            log_bash(f'cd "{self.cwd.absolute()}" && "{var}"')

            # get input
            test_input = bytes()
            if run_case.input:
                test_input, _ = self.exec_command(
                    run_case.input.get_command(), cwd=self.cwd
                )

            # run test
            test_output, test_err = self.exec_command(
                f"{self.run} {run_case.run_args}",
                cwd=self.cwd,
                input=test_input,
                exitcode=run_case.expected_exitcode,
            )

            # compare test and expected output
            if run_case.expected_output:
                test_expected_output, _ = self.exec_command(
                    run_case.expected_output.get_command(), cwd=self.cwd
                )
                if test_output != test_expected_output:
                    # For now...
                    print("", test_output, "\n", test_expected_output)
                    exit_with_error(
                        f"Stdouts do not match: {clamp_str(test_output.decode('UTF-8'))} != {clamp_str(test_expected_output.decode('UTF-8'))}"
                    )

            # compare test and expected err
            if run_case.expected_err:
                test_expected_err, _ = self.exec_command(
                    run_case.expected_err.get_command(), cwd=self.cwd
                )
                if test_err != test_expected_err:
                    # For now...
                    print("", test_err, "\n", test_expected_err)
                    exit_with_error(
                        f"Stderrs do not match: {clamp_str(test_err.decode('UTF-8'))} != {clamp_str(test_expected_err.decode('UTF-8'))}"
                    )

            # post run
            if self.post_run:
                self.exec_command(self.post_run, self.cwd, test_output)

    def exec_command(self, cmd, cwd: Path, input: bytes | None = None, exitcode=0):
        proc = sp.Popen(
            ["/bin/bash", "-c", cmd],
            cwd=cwd,
            stdin=sp.PIPE,
            stdout=sp.PIPE,
            stderr=sp.PIPE,
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


@dataclass
class TestSet:
    name: str
    subtests: list[Self]
    tests: list[Test]

    def __str__(self) -> str:
        return f"{self.name}: {len(self.tests)} tests"

    def __len__(self) -> int:
        return sum(len(test) for test in self.tests)


def load_run_case(test_dict: dict, run_case_name: str) -> RunCase:
    run_case_dict = test_dict["RunCases"][run_case_name]

    for var in run_case_dict:
        check_resembles_builtin(var, RUN_CASE_ALLOWED_KEYS)

    io_data = [None, None, None]
    for i, io in enumerate(["Input", "Output", "Err"]):
        if io in run_case_dict:
            io_data[i] = make_data_from_dict(run_case_dict[io])

    return RunCase(
        name=run_case_dict.get("Name", run_case_name),
        run_args=run_case_dict.get("RunArgs", ""),
        input=io_data[0],
        expected_output=io_data[1],
        expected_err=io_data[2],
        expected_exitcode=run_case_dict.get("ExitCode", 0),
        post_run=run_case_dict.get("PostRun", None),
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
                load_run_case(test_dict, run_case)
                for run_case in test_dict.get("RunCases", {})
            ],
            timeout=config_find_value(test_dict, "TimeOut", default=10),
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


def run_tests(tests: TestSet, tree=None):
    if tree is None:
        tree = [tests.name]
    else:
        tree.append(tests.name)

    for test in tests.tests:
        log_info(f"Testing: {'/'.join(tree + [test.name])}")
        test.run_test()

    for subtests in tests.subtests:
        run_tests(subtests, tree)


def integration_tests():
    log_info("Running integration tests...")
    config = load_config("tests")
    testset = load_subtest(config)
    run_tests(testset)
