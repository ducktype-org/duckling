import os
from pathlib import Path
from typing import Final

__QUACKPACK_CONFIG_LOCATION_RELATIVE_TO_CONFIG_DIR: Final[Path] = Path("duck") / "qp"


def get_config_file_directory() -> Path:
    """
    Get directory suitable for configuration file.
    ----
    Returns:
    - `pathlib.Path`: Path to directory for configuration file.
    """
    if dir := os.environ.get("QUACK_PACK_CONFIG"):
        return Path(dir)
    if dir := os.environ.get("XDG_CONFIG_HOME"):
        return Path(dir) / __QUACKPACK_CONFIG_LOCATION_RELATIVE_TO_CONFIG_DIR
    return Path.home() / ".config" / __QUACKPACK_CONFIG_LOCATION_RELATIVE_TO_CONFIG_DIR


def get_config_file() -> Path:
    """
    Get file location of configuration file.
    ----
    Returns:
    - `pathlib.Path`: Path to configuration file.
    """
    return get_config_file_directory() / "config.yaml"
