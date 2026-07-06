import pathlib

from ..helpers import (
    exec_bash_command,
    exit_with_error,
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


def dit_exec_command(
    command: str,
    cwd: pathlib.Path,
    capture_output: bool = True,
    input: bytes | None = None,
    exitcode: int = 0,
    dry: bool = False,
    verbose: bool = False,
    env: dict[str, str] | None = None,
    timeout: float | None = None,
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
        timeout=timeout,
    )
