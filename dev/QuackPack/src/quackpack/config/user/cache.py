from pathlib import Path
from typing import Annotated, Any, Final, Self

from pydantic import BaseModel, Field, field_serializer, model_validator

from quackpack.config.user.size import Size
from quackpack.util.default_pydantic_options import default_pydantic_options
from quackpack.util.xdg_fallbacks import cache_fallback_directory

_DEFAULT_MAX_CACHE_SIZE: Final[Size] = Size.from_string("10G")
_QUACKPACK_CACHE_LOCATION: Final[Path] = Path("duck") / "qp"

__all__ = ["CacheLocationEntry"]


def _default_cache_directory() -> Path:
    """
    Get default directory path for Quack Pack cache.
    ----
    Returns:
    - `pathlib.Path`: Default path to Quack Pack cache directory.
    """
    base_path = cache_fallback_directory()
    return base_path / _QUACKPACK_CACHE_LOCATION


class CacheLocationEntry(BaseModel):
    """
    Class representing cache entry in Quack Pack configuration.
    """

    # NOTE: Here we use plain Path, so we create directory also for default value (unlike in LocalEntry).
    path: Annotated[Path, Field(default=_default_cache_directory())]
    """
    Path to the Quack Pack cache directory.
    """
    max_size: Annotated[Size, Field(default=_DEFAULT_MAX_CACHE_SIZE)]
    """
    Maximum available size of Quack Pack cache directory.
    """
    model_config = default_pydantic_options()

    @model_validator(mode="after")
    def _create_directory_if_needed(self) -> Self:
        self.path.expanduser().mkdir(parents=True, exist_ok=True)
        return self

    @model_validator(mode="before")
    @classmethod
    def _convert_size(cls, serialized: Any) -> Any:
        key = "max_size"
        if not isinstance(serialized, dict):
            return serialized
        if key in serialized and isinstance(serialized[key], str):
            serialized[key] = Size.from_string(serialized[key])  # pyright: ignore[reportUnknownArgumentType]
        return serialized  # pyright: ignore[reportUnknownVariableType]

    @field_serializer("path", "max_size")
    def _serialize_as_str(self, value: Path | Size) -> str:
        return str(value)
