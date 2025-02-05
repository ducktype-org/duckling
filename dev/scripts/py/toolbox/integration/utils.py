from pathlib import Path
import pathlib
import sys
from ..helpers import (
    BashCommandError,
    click_log,
    exec_bash_command,
    exit_with_error,
    log_bash,
    replace_special,
)
import subprocess as sp


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
    if any(c.isupper() for c in name):
        exit_with_error(
            f"Unknown builtin `{name}`. Variables should be in snake_case or kebab-case."
        )


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


def dit_exec_command(
    command: str,
    cwd: pathlib.Path,
    capture_output=True,
    input: bytes | None = None,
    exitcode=0,
    dry: bool = False,
    verbose: bool = False,
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
    )


def write_log(msg, log_file):
    """
    Dumps a `msg` message into a log file `log_file`.
    """
    with open(log_file, "a") as f:
        print(">>>" + msg + f"{'-' * 50}", file=f)
