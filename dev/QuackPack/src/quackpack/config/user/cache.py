from os import getenv
from pathlib import Path
from typing import Any

from pydantic import BaseModel, Field, field_serializer, field_validator

from quackpack.config.user.size import Size
from quackpack.util.default_pydantic_options import DEFAULT_MODEL_CONFIG
from quackpack.util.xdg_directories import cache_directory

from ._relative_files_loc import QUACKPACK_CACHE_LOCATION


def _default_max_cache_size() -> Size:
    return Size.from_string("10G")


__all__ = ["Cache"]


def _default_cache_directory() -> Path:
    """
    Get default directory path for Quack Pack cache.
    ----
    Returns:
    - `pathlib.Path`: Default path to Quack Pack cache directory.
    """
    if directory := getenv("QP_CACHE"):
        return Path(directory)
    return cache_directory() / QUACKPACK_CACHE_LOCATION


class Cache(BaseModel):
    """
    Class representing cache entry in Quack Pack configuration.
    """

    # NOTE: Here we use plain Path, so we create directory also for default value (unlike in LocalEntry).
    download_dir: Path = Field(default_factory=lambda: _default_cache_directory() / "downloads")
    """
    Path to the Quack Pack fetcher cache directory.
    """

    # NOTE: Here we use plain Path, so we create directory also for default value (unlike in LocalEntry).
    artifacts_dir: Path = Field(default_factory=lambda: _default_cache_directory() / "artifacts")
    """
    Path to the Quack Pack fetcher artifacts directory.
    """

    metadata_db_path: Path = Field(default_factory=lambda: _default_cache_directory() / "metadata_db.sqlite")
    """
    Path to the Quack Pack metadata database.
    """

    fetcher_lockfile: Path = Field(default_factory=lambda: _default_cache_directory() / "fetcher.lock")
    """
    Path to the Quack Pack fetcher global lock file.
    """

    max_size: Size = Field(default_factory=_default_max_cache_size)
    """
    Maximum available size of Quack Pack cache directory.
    """

    model_config = DEFAULT_MODEL_CONFIG

    @field_validator("download_dir", mode="after")
    @classmethod
    def _create_directory_if_needed(cls, path: Path) -> Path:
        try:
            path.expanduser().mkdir(parents=True, exist_ok=True)
        except FileExistsError:
            raise NotADirectoryError(path) from None
        return path

    @field_validator("metadata_db_path", "fetcher_lockfile", mode="after")
    @classmethod
    def _create_file_if_needed(cls, path: Path) -> Path:
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
        except FileExistsError:
            raise NotADirectoryError(path.parent) from None
        path.touch(exist_ok=True)
        return path

    @field_validator("max_size", mode="before")
    @classmethod
    def _convert_size(cls, max_size: Any) -> Any:
        if not isinstance(max_size, str):
            return max_size
        return Size.from_string(max_size)

    @field_serializer("max_size")
    def _serialize_as_str(self, value: Size) -> str:
        return str(value)

    @field_serializer("download_dir", "metadata_db_path", "fetcher_lockfile")
    def _serialize_path(self, value: Path) -> str:
        return value.as_posix()
