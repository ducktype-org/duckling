from pathlib import Path
from dataclasses import dataclass
from typing import Optional, Self
from tomllib import loads

from ..helpers import exit_with_error, log_info

CONFIG_KEYS = {
    "name",
    "subtests",
    "tests",
    "build",
    "env",
    "config_file",
    "parent",
    "fail_fast",
}


@dataclass
class Config:
    config_file: Path
    name: str
    build: dict
    env: dict
    tests: list[dict]
    subtests: list[Self]
    parent: Optional[Self]
    fail_fast: bool


def make_test_name(name: str) -> str:
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


def load_config(dir_with_config: str, parent: Optional[dict] = None) -> dict:
    log_info(f"Loading config from {dir_with_config}")

    dir_with_config = Path(dir_with_config)
    config_file = dir_with_config / "testconfig.toml"
    if not config_file.exists():
        exit_with_error(f"Config file {config_file} does not exist")

    with open(config_file) as f:
        config = loads(f.read())

    if not "name" in config:
        config["name"] = dir_with_config.stem

    if not "fail_fast" in config:
        config["fail_fast"] = False

    # config["name"] = make_test_name(config["name"])
    config["config_file"] = config_file
    config["parent"] = parent

    if not ("subtests" in config or "tests" in config):
        exit_with_error(
            f"Config file {config_file} does not contain any tests or subtests"
        )

    if "subtests" in config:
        config["subtests"] = [
            load_config(dir_with_config / subtest, parent=config)
            for subtest in config["subtests"]
        ]

    if "tests" in config:
        for test_name in config["tests"]:
            test = config["tests"][test_name]
            # test["name"] = make_test_name(test_name)
            test["name"] = test_name

    if unknown_keys := set(config.keys()).difference(CONFIG_KEYS):
        exit_with_error(f"Unknown keys: {unknown_keys} in config file {config_file}")

    return config


def config_find_keys(config: dict, *keys) -> Optional[dict]:
    dictionary = config
    result = None
    for key in keys:
        if key in dictionary:
            result = dictionary[key]
            dictionary = result
    if result:
        return result

    if config["parent"]:
        return config_find_keys(config["parent"], *keys)
    return None
