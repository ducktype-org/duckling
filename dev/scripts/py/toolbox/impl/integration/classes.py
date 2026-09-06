from dataclasses import dataclass
from pathlib import Path
from typing import Optional, Self

from .io_data import IOData


@dataclass
class Case:
    """
    Test's Case object that represents data needed to run a test case.
    All variables are evaluated.
    """

    name: str
    enabled: str
    run: str
    pre_case: str
    post_case: str
    env: dict[str, str]
    input: Optional[IOData]
    expected_exitcode: int
    expected_output: Optional[IOData]
    expected_err: Optional[IOData]
    needed_threads: int
    timeout: int


@dataclass
class Test:
    """
    Represents a DIT test.
    Contains all the necessary data for running a Test.
    All variables are evaluated.
    """

    name: str
    description: Optional[str]
    cases: list[Case]
    cwd: Path

    pre_test: str
    post_test: str
    fail_fast: bool
    clean: str
    # Run this test's cases with nothing else executing concurrently
    # (for cases sensitive to machine load, e.g. tight timeouts).
    no_parallel: bool

    def __str__(self) -> str:
        return f"{self.name}: {self.description if self.description else ''}"

    def __len__(self) -> int:
        return len(self.cases)


@dataclass
class TestNode:
    """
    Represents a node inside DIT tree (a testconfig.yaml file).
    It has links to other TestNode objects (its subdirectories) and
    has a list of tests specified in a given config.
    It also specifies pre_node and post_node scripts that are run
    before and after all tests in this node and its subnodes.
    """

    name: str
    subtests: list[Self]
    tests: list[Test]
    cwd: Path

    pre_node: str
    post_node: str

    def test_count(self):
        return len(self.tests) + sum(testset.test_count() for testset in self.subtests)

    def __str__(self) -> str:
        return f"{self.name}: {len(self.tests)} tests"
