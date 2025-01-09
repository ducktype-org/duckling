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


def make_test_name(name: str) -> str:
    # for c in name:
    #     if c == c.upper():
    #         exit_with_error(
    #             f"Name should be in snake_case or kebab-case, but not: {name}"
    #         )

    name = name.replace("tests", "")
    name = name.replace("Tests", "")
    name = name.replace("test", "")
    name = name.replace("Test", "")

    name = name.capitalize()
    name = name.replace("-", "_")
    name = name.replace(" ", "_")
    while "__" in name:
        name = name.replace("__", "_")
    i = name.find("_")
    while i != -1:
        name = name[:i] + " " + name[i + 1 :].capitalize()
        i = name.find("_")

    return name.lstrip().rstrip()


def check_resembles_builtin(name: str, builtin_set: set[str]):
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


def log_success(msg, file=sys.stdout):
    click_log("GOOD", msg, fg="green", file=file)


def log_failure(msg, file=sys.stdout):
    click_log("FAIL", msg, fg="red", file=file)


def exec_command(
    cmd,
    cwd: Path,
    redirect=True,
    input: bytes | None = None,
    exitcode=0,
    dry: bool = False,
    verbose: bool = False,
):
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
    with open(log_file, 'a') as f:
        print(msg, file=f)
