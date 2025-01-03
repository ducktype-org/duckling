from dataclasses import dataclass
from typing import Optional, Self
from pathlib import Path

from .config import config_find_key, make_test_name
from .runcase import RunCase, make_data_from_dict
from ..helpers import (
    bash_command,
    bash_command_get_output,
    log_info,
)


@dataclass
class Test:
    name: str
    description: Optional[str]
    pre_build_command: str
    clean_command: str
    compile: str
    run: str
    run_cases: list[RunCase]
    timeout_s: int

    def run_test(self):
        bash_command(self.compile)
        for i, run_case in enumerate(self.run_cases):
            log_info(f"Running test case {i} -- {self.name}/{run_case.name}")
            bash_command_get_output(
                f"{run_case.input.get_command()} | {self.run_program} {run_case.run_args}"
            )

    def __str__(self) -> str:
        return f"{self.name}: {self.description}"

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
    run_case_dict = test_dict["run_cases"][run_case_name]

    input = None
    if "Input" in run_case_dict:
        input = make_data_from_dict(run_case_dict["Input"])
    expected_output = None
    if "Output" in run_case_dict:
        expected_output = make_data_from_dict(run_case_dict["Output"])
    expected_err = None
    if "Err" in run_case_dict:
        expected_err = make_data_from_dict(run_case_dict["Err"])

    return RunCase(
        name=make_test_name(run_case_dict.get("Name", run_case_name)),
        run_args=run_case_dict.get("RunArgs", ""),
        input=input,
        expected_output=expected_output,
        expected_err=expected_err,
        expected_exitcode=run_case_dict.get("ExitCode", 0),
        post_run=run_case_dict.get("PostRun", None),
    )


def load_test(config: dict, test_name) -> Test:
    test_dict = config["Tests"][test_name]
    return Test(
        name=make_test_name(test_dict.get("Name", test_name)),
        description=test_dict.get("Description"),
        compile=test_dict.get("Compile", config_find_key(config, "Compile")),
        run=test_dict.get("Run", config_find_key(config, "Run")),
        post_run=test_dict.get("PostRun", config_find_key(config, "PostRun")),
        run_cases=[
            load_run_case(test_dict, run_case)
            for run_case in test_dict.get("run_cases", {})
        ],
        timeout_s=test_dict.get(
            "timeout_s", x if (x := config_find_key(config, "PostRun")) else 10
        ),
    )


def load_test_set(config: dict) -> TestSet:
    name = make_test_name(config["Name"])
    subtests = [load_test_set(subtest) for subtest in config.get("SubTests", [])]
    tests = [load_test(config, test) for test in config.get("Tests", [])]
    return TestSet(name, subtests, tests)


def run_tests(tests: TestSet, tree=None):
    if tree is None:
        tree = [tests.name]

    log_info(f"Running {tree}")

    for test in tests.tests:
        test.run()

    for subtests in tests.subtests:
        run_tests(subtests, tree)
