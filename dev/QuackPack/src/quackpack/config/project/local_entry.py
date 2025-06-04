from pathlib import Path
from typing import override

from pydantic import BaseModel, field_serializer, field_validator

from quackpack.util.default_pydantic_options import DEFAULT_MODEL_CONFIG


class LocalEntry(BaseModel):
    """
    Local source of the dependency.
    """

    # NOTE: We use plain Path with after validator, so users can use `~`.
    path: Path
    """
    Local path to the dependency.
    """

    model_config = DEFAULT_MODEL_CONFIG

    @field_serializer("path")
    def _serialize_path(self, value: Path) -> str:
        return value.as_posix()

    # FIXME: Czy w walidatorze powinniśmy sprawdzać, że manifest istnieje w danej ścieżce?
    # FIXME: Czy coś powinno sprawdzać, że project pod LocalEntry nazywa się tak samo jak zależność w Dependencies?
    @field_validator("path", mode="after")
    @classmethod
    def _check_if_path_is_dir(cls, path: Path) -> Path:
        if not path.expanduser().is_dir():
            raise ValueError(f"'{path}' is not a directory")
        return path

    @override
    def __hash__(self):
        return hash(self.path)

    @override
    def __eq__(self, other: object) -> bool:
        if not isinstance(other, LocalEntry):
            return False
        return self.path == other.path
