from pathlib import Path
from typing import Final

from quackpack.util.env import Env
from quackpack.util.xdg_directories import cache_directory, config_directory, data_directory

SHOULD_BUILD_FROM_SOURCE: Final[bool] = False
DEFAULT_BUILD_PROFILE: Final[str] = "debug"
DEFAULT_DUCKC_BINARY: Final[str] = "duckc"

DEFAULT_REGISTRY_URL: Final[str] = "http://localhost:9001"

DEFAULT_STORAGE_TMP_LIFETIME_SECONDS: Final[int] = 60 * 60 * 24

SHOULD_FIX_TYPOS: Final[bool] = False
DEFAULT_TYPO_TOLERANCE_DISTANCE: Final[int] = 2

QUACKPACK_CACHE_LOCATION: Final[Path] = Path("qp")
QUACKPACK_CONFIG_LOCATION: Final[Path] = Path("qp")
QUACKPACK_DATA_LOCATION: Final[Path] = Path("qp")


def download_dir(env: Env) -> Path:
    return default_cache_directory(env) / "downloads"


def artifacts_dir(env: Env) -> Path:
    return default_cache_directory(env) / "artifacts"


def metadata_db(env: Env) -> Path:
    return default_cache_directory(env) / "metadata_db.sqlite"


def fetcher_lockfile(env: Env) -> Path:
    return default_cache_directory(env) / "fetcher.lock"


def storage_dir(env: Env) -> Path:
    return default_storage_directory(env) / "storage"


def global_venv_dir(env: Env) -> Path:
    return default_global_venv_directory(env) / "global_venv"


def default_cache_directory(env: Env) -> Path:
    """
    Get default directory path for Quack Pack cache.
    ----
    Returns:
    - `pathlib.Path`: Default path to Quack Pack cache directory.
    """
    if directory := env.get("QP_CACHE"):
        return Path(directory)
    return cache_directory(env) / QUACKPACK_CACHE_LOCATION


def default_storage_directory(env: Env) -> Path:
    """
    Get default directory path for Quack Pack storage.
    ----
    Returns:
    - `pathlib.Path`: Default path to Quack Pack storage directory.
    """
    if directory := env.get("QP_STORAGE"):
        return Path(directory)
    return data_directory(env) / QUACKPACK_DATA_LOCATION


def default_global_venv_directory(env: Env) -> Path:
    """
    Get default directory path for Quack Pack global venv.
    ----
    Returns:
    - `pathlib.Path`: Default path to Quack Pack cache directory.
    """
    if directory := env.get("QP_GLOBAL_VENV"):
        return Path(directory)
    return data_directory(env) / QUACKPACK_DATA_LOCATION


def config_file_directory(env: Env) -> Path:
    if directory := env.get("QP_CONFIG"):
        return Path(directory)
    return config_directory(env) / QUACKPACK_CONFIG_LOCATION


def config_filepath(env: Env) -> Path:
    """
    Get path of the configuration file.
    ----
    Returns:
    - `pathlib.Path`: Path to the configuration file.
    """
    return config_file_directory(env) / "config.toml"
