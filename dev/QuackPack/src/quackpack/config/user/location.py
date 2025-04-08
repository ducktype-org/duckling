import os
from pathlib import Path
from typing import Final

from quackpack.util.xdg_fallbacks import config_fallback_directory

_QUACKPACK_CONFIG_LOCATION: Final[Path] = Path("duck") / "qp"


def _config_file_directory() -> Path:
    """
    Get directory path suitable for the configuration file.
    ----
    Returns:
    - `pathlib.Path`: Path to directory of the configuration file.
    """
    if directory := os.environ.get("QUACK_PACK_CONFIG"):
        return Path(directory)
    if directory := os.environ.get("XDG_CONFIG_HOME"):
        return Path(directory) / _QUACKPACK_CONFIG_LOCATION
    return config_fallback_directory() / _QUACKPACK_CONFIG_LOCATION


def config_filepath() -> Path:
    """
    Get path of the configuration file.
    ----
    Returns:
    - `pathlib.Path`: Path to the configuration file.
    """
    return _config_file_directory() / "config.yaml"
