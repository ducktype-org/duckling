from pathlib import Path
from dataclasses import dataclass
from typing import Optional, Self
from tomllib import loads

from .utils import make_test_name, resembles_builtin

from ..helpers import exit_with_error, log_info, log_warning

CONFIG_KEYS = {
    "Name",
    "Description",
    "Env",
    "Tests",
    "SubTests",
    "Compile",
    "Run",
    "PostRun",
    "TimeOut",
    "ExitCode",
    "FailFast",
}


@dataclass
class Config:
    config_file: Path
    name: str
    env: dict
    variables: dict
    tests: list[dict]
    subtests: list[Self]
    parent: Optional[Self]
    fail_fast: bool


def load_config(dir_with_config: str, parent: Optional[dict] = None) -> dict:
    log_info(f"Loading config from {dir_with_config}")

    dir_with_config = Path(dir_with_config)
    config_file = dir_with_config / "testconfig.toml"
    if not config_file.exists():
        exit_with_error(f"Config file {config_file} does not exist")

    with open(config_file) as f:
        config = loads(f.read())

    config["Parent"] = parent
    config["ConfigFile"] = config_file
    config["Name"] = config.get("Name", dir_with_config.stem)
    config["FailFast"] = config.get("FailFast", False)

    if not ("SubTests" in config or "Tests" in config):
        exit_with_error(
            f"Config file {config_file} does not contain any Tests or SubTests"
        )

    if "SubTests" in config:
        config["SubTests"] = [
            load_config(dir_with_config / subtest, parent=config)
            for subtest in config["SubTests"]
        ]

    if "Tests" in config:
        for test_name in config["Tests"]:
            test = config["Tests"][test_name]
            test["Name"] = test_name

    for var in config:
        if not resembles_builtin(var, CONFIG_KEYS) and not var in CONFIG_KEYS:
            if var[0].isupper():
                exit_with_error(f"Variables should be in snake_case or kebab-case, not: {var}")

    return config


def config_find_key(config: dict, *keys) -> Optional[dict]:
    dictionary = config
    result = None
    for key in keys:
        if key in dictionary:
            result = dictionary[key]
            dictionary = result
    if result:
        return result

    if config["Parent"]:
        return config_find_key(config["Parent"], *keys)
    return None
