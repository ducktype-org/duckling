from pathlib import Path
import re
import tomllib
from typing import Optional
import yaml

from .utils import ExpressionFillError, VariableNotFound, check_resembles_builtin

from ..helpers import clamp_str, exit_with_error

CONFIG_KEYS = {
    "Name",
    "Description",
    "Env",
    "Tests",
    "Subtests",
    "Compile",
    "Run",
    "Clean",
    "PostRun",
    "TimeOut",
    "ExitCode",
}


# @dataclass
# class Config:
#     config_file: Path
#     name: str
#     env: dict
#     variables: dict
#     tests: list[dict]
#     subtests: list[Self]
#     parent: Optional[Self]
#     fail_fast: bool
#     time_out: int


def get_config_dict(dir_with_config: Path, ext="yaml"):
    config_file = dir_with_config / f"testconfig.{ext}"
    if not config_file.exists():
        exit_with_error(f"Config file {config_file} does not exist")

    with open(config_file) as f:
        match ext:
            case "yaml":
                config = yaml.safe_load(f.read())
            case "toml":
                config = tomllib.loads(f.read())

    config["_ConfigFile"] = config_file
    return config


def load_config(dir_with_config: str, parent: Optional[dict] = None) -> dict:
    dir_with_config = Path(dir_with_config)
    if not dir_with_config.exists():
        exit_with_error(f"Directory {dir_with_config.absolute()} does not exist")

    config = get_config_dict(dir_with_config)

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

            for run_case in test["RunCases"]:
                test["RunCases"][run_case]["_Parent"] = config["Tests"][test_name]

    return config


def config_find_value(config: dict, key, default=None) -> Optional[dict]:
    if key in config:
        return config[key]

    if config["_Parent"]:
        return config_find_value(config["_Parent"], key, default=default)
    return default


VARIABLE_EXPRESSION = re.compile(r"@{(.*?)}")


def config_fill_variables(config: dict, expr) -> str:
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


def config_find_and_fill(config, key):
    value = config_find_value(config, key)
    if value:
        return config_fill_variables(config, value)
    return value


def config_get_name_path(config):
    return (
        (config_get_name_path(parent) if (parent := config["_Parent"]) else "")
        + "/"
        + config["Name"]
    )
