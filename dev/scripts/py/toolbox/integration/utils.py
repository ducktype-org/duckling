
import sys
from ..helpers import click_log, exit_with_error


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
