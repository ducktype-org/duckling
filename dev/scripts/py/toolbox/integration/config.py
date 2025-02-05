from pathlib import Path
import re
import tomllib
from typing import Optional
import yaml

from .utils import ExpressionFillError, VariableNotFound, check_resembles_builtin

from ..helpers import clamp_str, exit_with_error

"""
Allowed global builtin keys.
"""
CONFIG_KEYS = {
    "Name",
    "Description",
    "Tests",
    "Subtests",
    "Compile",
    "Run",
    "Clean",
    "PostRun",
    "TimeOut",
    "ExitCode",
}


def read_config_dict(dir_with_config: Path):
    """
    Reads a DIT config file from a directory.
    """
    config_file = dir_with_config / f"testconfig.yaml"
    if not config_file.exists():
        exit_with_error(f"Config file {config_file} does not exist")

    with open(config_file) as f:
            config = yaml.safe_load(f.read())

    config["_ConfigFile"] = config_file
    return config


def load_config(dir_with_config: str, parent: Optional[dict] = None) -> dict:
    """
    Loads a DIT config from a `dir_with_config` recursively.
    Does error checking and fills the config structure.
    """
    dir_with_config = Path(dir_with_config)
    if not dir_with_config.exists():
        exit_with_error(f"Directory {dir_with_config.absolute()} does not exist")

    config = read_config_dict(dir_with_config)

    for var in config:
        check_resembles_builtin(var, CONFIG_KEYS)

    config["_Parent"] = parent
    config["Name"] = config.get("Name", dir_with_config.stem)

    if not ("Subtests" in config or "Tests" in config):
        exit_with_error(
            f"Config file {config["_ConfigFile"]} does not contain any Tests or Subtests"
        )

    if "Subtests" in config:
        config["Subtests"] = [
            load_config(dir_with_config / subtest, parent=config)
            for subtest in config["Subtests"]
        ]

    if "Tests" in config:
        for test_name in config["Tests"]:
            test = config["Tests"][test_name]
            test["Name"] = test_name
            test["_Parent"] = config

            for case in test["Cases"]:
                test["Cases"][case]["_Parent"] = config["Tests"][test_name]

    return config


def config_find_value(config: dict, key, default=None) -> Optional[dict]:
    """
    Looks for a `key` inside `config` and upon finding returns it.
    If it doesn't find, then goes up tree inside the config structure.
    """
    if key in config:
        return config[key]

    if config["_Parent"]:
        return config_find_value(config["_Parent"], key, default=default)
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

    raise ExpressionFillError(clamp_str(ORIGINAL_EXPR, 100))


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
        (config_get_name_path(parent) if (parent := config["_Parent"]) else "")
        + "/"
        + config["Name"]
    )
