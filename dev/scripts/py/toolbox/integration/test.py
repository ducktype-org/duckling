from dataclasses import dataclass
from typing import Optional, Self
from pathlib import Path

from .config import config_find_keys
from .runcase import RunCase, make_data_from_dict
from ..helpers import (
    bash_command,
    bash_command_get_output,
    log_info,
    make_singleline_command,
)

TEST_KEYS = {
    "name",
    "description",
    "pre_build_command",
    "clean_command",
    "compiler",
    "compile_file",
    "compile_args",
    "compile_args_misc",
    "run_program",
    "run_cases",
    "timeout_s",
}


@dataclass
class Test:
    name: str
    description: Optional[str]
    pre_build_command: str
    clean_command: str
    compiler: str
    compile_file: str
    compile_args: str
    compile_args_misc: str
    run_program: str
    run_cases: list[RunCase]
    timeout_s: int

    @property
    def compile(self) -> str:
        return make_singleline_command(
            f"{self.compiler} {self.compile_file} {self.compile_misc_args} {self.compile_args}"
        )

    def run(self):
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
    if "input" in run_case_dict:
        input = make_data_from_dict(run_case_dict["input"])
    expected_output = None
    if "output" in run_case_dict:
        expected_output = make_data_from_dict(run_case_dict["output"])
    expected_err = None
    if "err" in run_case_dict:
        expected_err = make_data_from_dict(run_case_dict["err"])

    return RunCase(
        name=run_case_dict.get("name", run_case_name),
        run_args=run_case_dict.get("run_args", ""),
        input=input,
        expected_output=expected_output,
        expected_err=expected_err,
        expected_exitcode=run_case_dict.get("exitcode", 0),
        post_run_command=run_case_dict.get("post_run_command", None),
    )


def load_test(config: dict, test_name) -> Test:
    test_dict = config["tests"][test_name]
    test = Test(
        name=test_name,
        description=test_dict.get("description"),
        pre_build_command=test_dict.get("pre_build_command", ""),
        clean_command=test_dict.get("clean_command", ""),
        compiler=config_find_keys(config, "build", "compiler"),
        compile_file=test_dict["compile_file"],
        compile_args=test_dict.get(
            "compile_args", config_find_keys(config, "build", "compile_args")
        ),
        compile_args_misc=test_dict.get(
            "compile_args_misc",
        ),
        run_program=test_dict["run_program"],
        run_cases=[
            load_run_case(test_dict, run_case) for run_case in test_dict["run_cases"]
        ],
        timeout_s=test_dict.get("timeout_s", 10),
    )
    name = test_dict["name"]
    description = test_dict.get("description")


def load_test_set(config: dict) -> TestSet:
    name = config["name"]
    subtests = [load_test_set(subtest) for subtest in config.get("subtests", [])]
    tests = [load_test(config, test) for test in config.get("tests", [])]
    return TestSet(name, subtests, tests)


def run_tests(tests: TestSet, tree=None):
    if tree is None:
        tree = [tests.name]

    log_info(f"Running {tree}")

    for test in tests.tests:
        test.run()

    run_tests(tests.subtests, tree)
