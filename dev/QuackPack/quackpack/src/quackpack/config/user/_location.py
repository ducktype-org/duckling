import os
from pathlib import Path
from typing import Final

__QUACKPACK_CONFIG_LOCATION_RELATIVE_TO_CONFIG_DIR: Final[Path] = Path("duck") / "qp"


def _config_file_directory() -> Path:
    """
    Get directory path suitable for the configuration file.
    ----
    Returns:
    - `pathlib.Path`: Path to directory of the configuration file.
    """
    if dir := os.environ.get("QUACK_PACK_CONFIG"):
        return Path(dir)
    if dir := os.environ.get("XDG_CONFIG_HOME"):
        return Path(dir) / __QUACKPACK_CONFIG_LOCATION_RELATIVE_TO_CONFIG_DIR
    return Path.home() / ".config" / __QUACKPACK_CONFIG_LOCATION_RELATIVE_TO_CONFIG_DIR


def config_filepath() -> Path:
    """
    Get path of the configuration file.
    ----
    Returns:
    - `pathlib.Path`: Path to the configuration file.
    """
    return _config_file_directory() / "config.yaml"
