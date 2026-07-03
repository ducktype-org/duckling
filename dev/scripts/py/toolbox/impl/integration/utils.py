import pathlib
import sys
import threading
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


_WRITE_LOG_LOCK = threading.Lock()


def write_log(msg, log_file):
    """
    Dumps a `msg` message into a log file `log_file`.
    """
    with _WRITE_LOG_LOCK:
        with open(log_file, "a") as f:
            print(">>>" + msg + f"{'-' * 50}", file=f)


def log_info_if_needed(msg: str, dry: bool, verbose: bool):
    if dry or verbose:
        log_info(msg)


class CaseLog:
    """
    Collects the console lines and log-file chunks of a single test case.
    In immediate mode (sequential runs) everything is emitted right away;
    otherwise the output is buffered and `flush` emits it in one piece
    under `lock`, so concurrently running cases stay readable.
    """

    def __init__(
        self,
        log_file: pathlib.Path,
        immediate: bool = True,
        lock: threading.Lock | None = None,
    ):
        self.log_file = log_file
        self.immediate = immediate
        self.lock = lock
        self.console = []
        self.chunks = []

    def emit(self, print_fn, msg: str):
        if self.immediate:
            print_fn(msg)
        else:
            self.console.append((print_fn, msg))

    def info_if_needed(self, msg: str, dry: bool, verbose: bool):
        if dry or verbose:
            self.emit(log_info, msg)

    def log(self, msg: str):
        if self.immediate:
            write_log(msg, self.log_file)
        else:
            self.chunks.append(msg)

    def flush(self):
        if self.immediate:
            return
        with self.lock:
            for print_fn, msg in self.console:
                print_fn(msg)
        for chunk in self.chunks:
            write_log(chunk, self.log_file)


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
