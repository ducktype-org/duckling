from os import getenv
from pathlib import Path

from pydantic import BaseModel, Field, PositiveInt, field_serializer, field_validator

from quackpack.util.default_pydantic_options import DEFAULT_MODEL_CONFIG
from quackpack.util.xdg_directories import data_directory

from ._relative_files_loc import QUACKPACK_DATA_LOCATION

__all__ = ["StorageEntry"]


def _default_storage_directory() -> Path:
    """
    Get default directory path for Quack Pack storage.
    ----
    Returns:
    - `pathlib.Path`: Default path to Quack Pack storage directory.
    """
    if directory := getenv("QP_STORAGE"):
        return Path(directory)
    return data_directory() / QUACKPACK_DATA_LOCATION


class StorageEntry(BaseModel):
    """
    Class representing storage entry in Quack Pack configuration.
    """

    # NOTE: Here we use plain Path, so we create directory also for default value (unlike in LocalEntry).
    storage: Path = Field(default_factory=lambda: _default_storage_directory() / "storage")
    """
    Path to the Quack Pack storage directory.
    """

    temporary_lifetime: PositiveInt = 60 * 60 * 24
    """
    Time in seconds after which a temporary virtual environment may be deleted during clean operation.
    """

    model_config = DEFAULT_MODEL_CONFIG

    @field_validator("storage", mode="after")
    @classmethod
    def _create_directory_if_needed(cls, path: Path) -> Path:
        try:
            path.expanduser().mkdir(parents=True, exist_ok=True)
        except FileExistsError:
            raise NotADirectoryError(path) from None
        return path

    @field_serializer("storage")
    def _serialize_path(self, value: Path) -> str:
        return value.as_posix()
