import pathlib
import sys
from dataclasses import dataclass, astuple

from ..helpers import (
    click_log,
    exec_bash_command,
    exit_with_error,
    log_info,
)


def assert_good_var_name(name: str):
    if any(c.isupper() for c in name):
        exit_with_error(
            f"Invalid variable name `{name}`. Variables should be in snake_case or kebab-case."
        )


def check_resembles_builtin(name: str, builtin_set: set[str]):
    """
    Checks whether `name`, which is a variable name, is a misspelling of a builtin from a set of builtins.
    Also checks whether a variable follows the correct naming convention.
    """
    if name in builtin_set:
        return

    for key in builtin_set:
        if name.lower() == key.lower() and name != key:
            exit_with_error(
                f"Incorrect spelling of '{name}' in config file. Consider: '{key}'"
            )
    assert_good_var_name(name)


class VariableNotFound(Exception):
    def __init__(self, variable_name, expr):
        super().__init__(f"Cannot resolve: `{variable_name}` for expression: {expr}.")
        self.variable_name = variable_name
        self.expr = expr


class ExpressionFillError(Exception):
    def __init__(self, expr):
        super().__init__(
            f"Couldn't resolve {expr}. Possible usage of cyclic variables."
        )
        self.expr = expr


def print_success(msg, file=sys.stdout):
    click_log("GOOD", msg, fg="green", file=file)


def print_failure(msg, file=sys.stdout):
    click_log("FAIL", msg, fg="red", file=file)


def print_neutral(msg, file=sys.stdout):
    click_log("INFO", msg, fg="white", file=file)


def dit_exec_command(
    command: str,
    cwd: pathlib.Path,
    capture_output=True,
    input: bytes | None = None,
    exitcode=0,
    dry: bool = False,
    verbose: bool = False,
    env: dict[str, str] | None = None,
) -> tuple[bytes, bytes]:
    return exec_bash_command(
        command=command,
        cwd=cwd,
        capture_output=capture_output,
        input=input,
        exitcode=exitcode,
        dry=dry,
        verbose=verbose,
        decode=False,
        env=env,
    )


def write_log(msg, log_file):
    """
    Dumps a `msg` message into a log file `log_file`.
    """
    with open(log_file, "a") as f:
        print(">>>" + msg + f"{'-' * 50}", file=f)


def log_info_if_needed(msg: str, dry: bool, verbose: bool):
    if dry or verbose:
        log_info(msg)


@dataclass
class Success:
    pass


@dataclass
class Failure:
    error: str


@dataclass
class Disabled:
    pass


@dataclass
class TestStatistics:
    """
    Describes the number of ran and succeeded, failed, disabled test cases.
    """

    succeeded: list[str]
    failed: list[str]
    disabled: list[str]

    def __add__(self, other):
        return TestStatistics(
            self.succeeded + other.succeeded,
            self.failed + other.failed,
            self.disabled + other.disabled,
        )

    def iadd(self, other):
        self.failed += other.failed
        self.succeeded += other.succeeded
        self.disabled += other.disabled

    def __iter__(self):
        return iter(astuple(self))
