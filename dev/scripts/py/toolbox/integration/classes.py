
from dataclasses import dataclass
from pathlib import Path
from typing import Optional, Self

from .io_data import IOData


@dataclass
class Case:
    """
    Test's Case object that represents data needed to run a test case.
    """

    name: str
    run_args: str
    input: Optional[IOData]
    expected_exitcode: int
    expected_output: Optional[IOData]
    expected_err: Optional[IOData]
    timeout: int


@dataclass
class Test:
    """
    Represents a DIT test.
    Contains all the necessary data for running a Case.
    """

    name: str
    description: Optional[str]
    cases: list[Case]
    cwd: Path

    compile: str
    run: str
    post_run: str
    fail_fast: bool
    clean: str

    def __str__(self) -> str:
        return f"{self.name}: {self.description if self.description else ''}"

    def __len__(self) -> int:
        return len(self.cases)


@dataclass
class TestNode:
    """
    Represents a node inside DIT tree (a testconfig.yaml file).
    It has links to other TestNode objects (subdirectories) and
    has a list of tests specified in a given config.
    """

    name: str
    subtests: list[Self]
    tests: list[Test]

    def test_count(self):
        return len(self.tests) + sum(testset.test_count() for testset in self.subtests)

    def __str__(self) -> str:
        return f"{self.name}: {len(self.tests)} tests"
