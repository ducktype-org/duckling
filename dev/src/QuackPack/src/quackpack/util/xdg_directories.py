"""
General documentation is here: https://specifications.freedesktop.org/basedir-spec/latest/#variables,
but it assumes Linux only. More granular description can be found here: https://github.com/adrg/xdg/blob/master/README.md#xdg-base-directory.
But treat macOS as Linux/UNIX/POSIX. See comments for explanation.
"""

from pathlib import Path
from sys import platform

from quackpack.util.env import Env


def _config_fallback_directory(env: Env) -> Path:
    """
    Get proper fallback for XDG_CONFIG_HOME.
    """
    # Windows.
    if platform == "win32":
        local_app_data = env.get("LOCALAPPDATA")
        assert local_app_data is not None, "This should always exist on windows"
        return Path(local_app_data)
    # Not a Windows. Assume Linux compliant. (for macOS also use XDG spec, since Quack Pack is a CLI app).
    return Path.home() / ".config"


def _cache_fallback_directory(env: Env) -> Path:
    """
    Get proper fallback for XDG_CACHE_HOME.
    """
    # Windows.
    if platform == "win32":
        local_app_data = env.get("LOCALAPPDATA")
        assert local_app_data is not None, "This should always exist on windows"
        return Path(local_app_data) / "caches"
    # Not a Windows. Assume Linux compliant. (for macOS also use XDG spec, since Quack Pack is a CLI app).
    return Path.home() / ".cache"


def _data_fallback_directory(env: Env) -> Path:
    """
    Get proper fallback for XDG_DATA_HOME.
    """
    # Windows.
    if platform == "win32":
        local_app_data = env.get("LOCALAPPDATA")
        assert local_app_data is not None, "This should always exist on windows"
        return Path(local_app_data)
    # Not a Windows. Assume Linux compliant. (for macOS also use XDG spec, since Quack Pack is a CLI app).
    return Path.home() / ".local" / "share"


def _state_fallback_directory(env: Env) -> Path:
    """
    Get proper fallback for XDG_STATE_HOME.
    """
    # Windows.
    if platform == "win32":
        local_app_data = env.get("LOCALAPPDATA")
        assert local_app_data is not None, "This should always exist on windows"
        return Path(local_app_data)
    # Not a Windows. Assume Linux compliant. (for macOS also use XDG spec, since Quack Pack is a CLI app).
    return Path.home() / ".local" / "state"


def config_directory(env: Env) -> Path:
    """
    Get default directory for storing configuration files.
    """
    if directory := env.get("XDG_CONFIG_HOME"):
        return Path(directory)
    return _config_fallback_directory(env)


def cache_directory(env: Env) -> Path:
    """
    Get default directory for storing caches.
    """
    if directory := env.get("XDG_CACHE_HOME"):
        return Path(directory)
    return _cache_fallback_directory(env)


def data_directory(env: Env) -> Path:
    """
    Get default directory for any data.
    """
    if directory := env.get("XDG_DATA_HOME"):
        return Path(directory)
    return _data_fallback_directory(env)


def state_directory(env: Env) -> Path:
    """
    Get default directory for any state.
    """
    if directory := env.get("XDG_STATE_HOME"):
        return Path(directory)
    return _state_fallback_directory(env)
