"""
General documentation is here: https://specifications.freedesktop.org/basedir-spec/latest/#variables,
but it assumes Linux only. More granular description can be found here: https://github.com/adrg/xdg/blob/master/README.md#xdg-base-directory.
"""

from os import getenv
from pathlib import Path
from sys import platform


def config_fallback_directory() -> Path:
    """
    Get proper fallback for XDG_CONFIG_HOME.
    """
    # macOS.
    if platform == "darwin":
        return Path.home() / "Library" / "Application Support"
    # Windows.
    if platform == "win32":
        return Path(getenv("LOCALAPPDATA"))  # pyright: ignore[reportArgumentType]; this variable should always exist.
    # Anything else. Assume Linux compliant.
    return Path.home() / ".config"


def cache_fallback_directory() -> Path:
    """
    Get proper fallback for XDG_CACHE_HOME.
    """
    # macOS.
    if platform == "darwin":
        return Path.home() / "Library" / "Caches"
    # Windows.
    if platform == "win32":
        return Path(getenv("LOCALAPPDATA")) / "caches"  # pyright: ignore[reportArgumentType]; this variable should always exist.
    # Anything else. Assume Linux compliant.
    return Path.home() / ".cache"


def data_fallback_directory() -> Path:
    """
    Get proper fallback for XDG_DATA_HOME.
    """
    # macOS.
    if platform == "darwin":
        return Path.home() / "Library" / "Application Support"
    # Windows.
    if platform == "win32":
        return Path(getenv("LOCALAPPDATA"))  # pyright: ignore[reportArgumentType]; this variable should always exist.
    # Anything else. Assume Linux compliant.
    return Path.home() / ".local" / "share"


def state_fallback_directory() -> Path:
    """
    Get proper fallback for XDG_STATE_HOME.
    """
    # macOS.
    if platform == "darwin":
        return Path.home() / "Library" / "Application Support"
    # Windows.
    if platform == "win32":
        return Path(getenv("LOCALAPPDATA"))  # pyright: ignore[reportArgumentType]; this variable should always exist.
    # Anything else. Assume Linux compliant.
    return Path.home() / ".local" / "state"
