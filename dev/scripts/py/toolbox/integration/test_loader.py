from dataclasses import dataclass
from pathlib import Path
from typing import Optional, Self
from ..helpers import exit_with_error
from .utils import ExpressionFillError, VariableNotFound, check_resembles_builtin
from .runcase import RunCase, make_data_from_dict
from .config import (
    CONFIG_KEYS,
    config_find_and_fill,
    config_find_value,
    config_get_name_path,
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

    def __str__(self) -> str:
        return f"{self.name}: {self.description if self.description else ''}"

    def __len__(self) -> int:
        return len(self.run_cases)


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
