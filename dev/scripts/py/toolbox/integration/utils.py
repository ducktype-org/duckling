from pathlib import Path
import sys
from ..helpers import (
    BashCommandError,
    click_log,
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
    if name[0].isupper():
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


def exec_command(
    cmd,
    cwd: Path,
    redirect=True,
    input: bytes | None = None,
    exitcode=0,
    dry: bool = False,
    verbose: bool = False,
) -> tuple[bytes, bytes]:
    """
    Similar to `bash_command_get_output` as it executes a bash command and returns the result.
    It has more features than the aforementioned command, as it supports input redirection,
    can expect a custom exitcode and supports dry and verbose runs.
    """
    if dry or verbose:
        cmd = replace_special(cmd)
        log_bash(f'cd "{cwd.absolute()}" && {cmd}')
        if dry:
            return bytes(), bytes()

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


def write_log(msg, log_file):
    """
    Dumps a `msg` message into a log file `log_file`.
    """
    with open(log_file, "a") as f:
        print(">>>" + msg + f"{'-' * 50}", file=f)
