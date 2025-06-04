"""
General documentation is here: https://specifications.freedesktop.org/basedir-spec/latest/#variables,
but it assumes Linux only. More granular description can be found here: https://github.com/adrg/xdg/blob/master/README.md#xdg-base-directory.
"""

from os import getenv
from pathlib import Path
from sys import platform


def _config_fallback_directory() -> Path:
    """
    Get proper fallback for XDG_CONFIG_HOME.
    """
    # macOS.
    if platform == "darwin":
        return Path.home() / "Library" / "Application Support"
    # Windows.
    if platform == "win32":
        local_app_data = getenv("LOCALAPPDATA")
        assert local_app_data is not None, "This should always exist on windows"
        return Path(local_app_data)
    # Anything else. Assume Linux compliant.
    return Path.home() / ".config"


def _cache_fallback_directory() -> Path:
    """
    Get proper fallback for XDG_CACHE_HOME.
    """
    # macOS.
    if platform == "darwin":
        return Path.home() / "Library" / "Caches"
    # Windows.
    if platform == "win32":
        local_app_data = getenv("LOCALAPPDATA")
        assert local_app_data is not None, "This should always exist on windows"
        return Path(local_app_data) / "caches"
    # Anything else. Assume Linux compliant.
    return Path.home() / ".cache"


def _data_fallback_directory() -> Path:
    """
    Get proper fallback for XDG_DATA_HOME.
    """
    # macOS.
    if platform == "darwin":
        return Path.home() / "Library" / "Application Support"
    # Windows.
    if platform == "win32":
        local_app_data = getenv("LOCALAPPDATA")
        assert local_app_data is not None, "This should always exist on windows"
        return Path(local_app_data)
    # Anything else. Assume Linux compliant.
    return Path.home() / ".local" / "share"


def _state_fallback_directory() -> Path:
    """
    Get proper fallback for XDG_STATE_HOME.
    """
    # macOS.
    if platform == "darwin":
        return Path.home() / "Library" / "Application Support"
    # Windows.
    if platform == "win32":
        local_app_data = getenv("LOCALAPPDATA")
        assert local_app_data is not None, "This should always exist on windows"
        return Path(local_app_data)
    # Anything else. Assume Linux compliant.
    return Path.home() / ".local" / "state"


def config_directory() -> Path:
    """
    Get default directory for storing configuration files.
    """
    if directory := getenv("XDG_CONFIG_HOME"):
        return Path(directory)
    return _config_fallback_directory()


def cache_directory() -> Path:
    """
    Get default directory for storing caches.
    """
    if directory := getenv("XDG_CACHE_HOME"):
        return Path(directory)
    return _cache_fallback_directory()


def data_directory() -> Path:
    """
    Get default directory for any data.
    """
    if directory := getenv("XDG_DATA_HOME"):
        return Path(directory)
    return _data_fallback_directory()


def state_directory() -> Path:
    """
    Get default directory for any state.
    """
    if directory := getenv("XDG_STATE_HOME"):
        return Path(directory)
    return _state_fallback_directory()
