from pathlib import Path
import re
from typing import Optional
import yaml

from .utils import ExpressionFillError, VariableNotFound, check_resembles_builtin

from ..helpers import truncate_str, exit_with_error
from keys import *

"""
General variable keys.
"""
GENERAL_VARIABLES = {
    COMPILE,
    RUN,
    CLEAN,
    POST_RUN,
    TIME_OUT,
    EXIT_CODE,
}

"""
Allowed global builtin keys.
"""
GLOBAL_CONFIG_KEYS = {
    NAME,
    DESCRIPTION,
    TESTS,
    SUBTESTS,
    *GENERAL_VARIABLES
}


def _read_config_file(dir_with_config: Path) -> dict:
    """
    Reads a DIT config file from a directory.
    """
    config_file = dir_with_config / "testconfig.yaml"
    if not config_file.exists():
        exit_with_error(f"Config file {config_file} does not exist")

    with open(config_file) as f:
        config = yaml.safe_load(f.read())

    config[CONFIG_FILE] = config_file
    return config


def load_config(dir_with_config: str, parent: Optional[dict] = None) -> dict:
    """
    Loads a DIT config from a `dir_with_config` recursively.
    Does error checking and fills the config structure.
    """
    dir_with_config = Path(dir_with_config)
    if not dir_with_config.exists():
        exit_with_error(f"Directory {dir_with_config.absolute()} does not exist")

    config = _read_config_file(dir_with_config)

    for var in config:
        check_resembles_builtin(var, GLOBAL_CONFIG_KEYS)

    config[PARENT] = parent
    config[NAME] = config.get(NAME, dir_with_config.stem)

    if not (SUBTESTS in config or TESTS in config):
        exit_with_error(
            f"Config file {config[CONFIG_FILE]} does not contain any {TESTS} or {SUBTESTS}"
        )

    if SUBTESTS in config:
        config[SUBTESTS] = [
            load_config(dir_with_config / subtest, parent=config)
            for subtest in config[SUBTESTS]
        ]

    if TESTS in config:
        for test_name in config[TESTS]:
            test = config[TESTS][test_name]
            test[NAME] = test_name
            test[PARENT] = config

            for case in test[CASES]:
                test[CASES][case][PARENT] = config[TESTS][test_name]

    return config


def config_find_value(config: dict, key: str, default=None) -> Optional[dict]:
    """
    Looks for a `key` inside `config` and upon finding returns it.
    If it doesn't find, then goes up tree inside the config structure.
    """
    if key in config:
        return config[key]

    if config[PARENT]:
        return config_find_value(config[PARENT], key, default=default)
    return default


VARIABLE_EXPRESSION = re.compile(r"@{(.*?)}")


def config_fill_variables(config: dict, expr: str) -> str:
    """
    Evaluates an expression based on config.
    """
    ORIGINAL_EXPR = expr
    MAX_ITER = 10

    def repl_var(matchobj):
        variable_name = matchobj.group(1)
        if value := config_find_value(config, variable_name):
            return value

        raise VariableNotFound(variable_name, expr)

    for _ in range(MAX_ITER):
        expr = VARIABLE_EXPRESSION.sub(repl_var, expr)

        if not VARIABLE_EXPRESSION.search(expr):
            return expr

    raise ExpressionFillError(truncate_str(ORIGINAL_EXPR, 100))


def config_find_and_fill(config: dict, key: str) -> Optional[str]:
    """
    Finds a value of a `key` inside `config` and tries to evaluate
    its expression.
    """
    value = config_find_value(config, key)
    if value:
        return config_fill_variables(config, value)
    return value


def config_get_name_path(config: dict) -> str:
    """
    Builds a string representing a test tree up to `config`'s node.
    An example would be "/tests/a/b/c".
    """
    return (
        (config_get_name_path(parent) if (parent := config[PARENT]) else "")
        + "/"
        + config[NAME]
    )
