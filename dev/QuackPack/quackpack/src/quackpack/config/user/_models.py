# pyright: standard
from __future__ import annotations

import os
import sys
from enum import StrEnum
from pathlib import Path
from typing import Annotated, Any, Final

import yaml
from pydantic import BaseModel, ConfigDict, Field, field_serializer, model_validator
from pydantic.alias_generators import to_pascal
from pydantic.config import ExtraValues

from quackpack.logger import get_logger

from ._location import get_config_file

_logger = get_logger(__name__)

PathLike = str | Path


class Size(BaseModel):
    """
    Class representing disk size with human interface.
    """

    class Unit(StrEnum):
        GiB = "G"
        MiB = "M"
        KiB = "K"

    size: int = Field(gt=0)
    unit: Unit

    @staticmethod
    def from_string(size: str) -> Size:
        """
        Pass `size` as a `Size`.
        ----
        Args:
        - `size`: string to be parsed.
        ----
        Returns:
        - `Size`: human parsed size.
        ----
        Raises:
        - `ValueError`: if `size` couldn't be parsed.
        """
        try:
            unit = Size.Unit(size[-1])
            size_as_int = int(size[:-1])
            data = {"size": size_as_int, "unit": unit}
            return Size.model_validate(data)
        except Exception as e:
            raise ValueError(
                f"'{size}' is not a valid size of format <UINT><SUFFIX = ('G', 'M', 'K')>"
            ) from e

    def __str__(self) -> str:
        return f"{self.size}{self.unit}"


__DEFAULT_MAX_CACHE_SIZE: Final[Size] = Size.from_string("10G")
__QUACKPACK_CACHE_LOCATION_RELATIVE_TO_CACHE_DIR: Final[Path] = Path("duck") / "qp"
__DEFAULT_TYPO_TOLERANCE: Final[int] = 10
__ALLOW_EXTRA_ARGS: Final[ExtraValues] = "ignore"


def _get_default_model_config():
    return ConfigDict(extra=__ALLOW_EXTRA_ARGS, alias_generator=to_pascal)


def _get_default_cache_directory() -> Path:
    """
    Get default directory path for Quack Pack cache.
    ----
    Returns:
    - `pathlib.Path`: Default path to Quack Pack cache directory.
    """
    base_path = Path(dir) if (dir := os.getenv("XDG_CACHE_HOME")) else Path.home() / ".cache"
    return base_path / __QUACKPACK_CACHE_LOCATION_RELATIVE_TO_CACHE_DIR


class CacheEntry(BaseModel):
    """
    Class representing cache entry in Quack Pack configuration.
    """

    path: Annotated[Path, Field(default=_get_default_cache_directory())]
    max_size: Annotated[Size, Field(default=__DEFAULT_MAX_CACHE_SIZE)]
    model_config = _get_default_model_config()

    @model_validator(mode="before")
    @classmethod
    def convert_size(cls, values: Any) -> Any:
        if not isinstance(values, dict):
            return values
        if "MaxSize" in values and isinstance(values["MaxSize"], str):
            values["MaxSize"] = Size.from_string(values["MaxSize"])
        return values

    @field_serializer("path")
    def serialize_path(self, value: Path) -> str:
        return str(value)

    @field_serializer("max_size")
    def serialize_size(self, size: Size) -> str:
        return str(size)


class PackagingEntry(BaseModel):
    """
    Class representing packaging entry in Quack Pack configuration.
    """

    build_from_source: Annotated[bool, Field(default=False)]
    model_config = _get_default_model_config()
    # TODO: Add dependency solver.


class BuildEntry(BaseModel):
    """
    Class representing build entry in Quack Pack configuration.
    """

    extra_flags: Annotated[str, Field(default="")]
    model_config = _get_default_model_config()


class RepositoryEntry(BaseModel):
    """
    Class representing repository entry in Quack Pack configuration.
    """

    override: Annotated[str | list[str] | None, Field(default=None)]
    extra: Annotated[str | list[str] | None, Field(default=None)]
    model_config = _get_default_model_config()


class TypoTolerance(BaseModel):
    """
    Class representing Typo tolerance entry in Quack Pack configuration.
    """

    enabled: Annotated[bool, Field(default=False)]
    max_distance: Annotated[int, Field(default=__DEFAULT_TYPO_TOLERANCE)]
    model_config = _get_default_model_config()


class Security(BaseModel):
    """
    Class representing security entry in Quack Pack configuration.
    """

    # They are callable, because Python is great language, so let's just silence errors.
    typo_tolerace: Annotated[TypoTolerance, Field(default_factory=TypoTolerance)]  # pyright: ignore[reportArgumentType]
    model_config = _get_default_model_config()


class Config(BaseModel):
    """
    Class representing global Quack Pack configuration.
    """

    # They are callable, because Python is great language, so let's just silence errors.
    aliases: Annotated[dict[str, str | list[str]], Field(default={})]
    cache: Annotated[CacheEntry, Field(default_factory=CacheEntry)]  # pyright: ignore[reportArgumentType]
    packaging: Annotated[PackagingEntry, Field(default_factory=PackagingEntry)]  # pyright: ignore[reportArgumentType]
    build: Annotated[BuildEntry, Field(default_factory=BuildEntry)]  # pyright: ignore[reportArgumentType]
    repository: Annotated[RepositoryEntry, Field(default_factory=RepositoryEntry)]  # pyright: ignore[reportArgumentType]
    security: Annotated[Security, Field(default_factory=Security)]  # pyright: ignore[reportArgumentType]
    _location: Path
    model_config = _get_default_model_config()

    def __init__(self, **data) -> None:
        super().__init__(**data)
        self._location = data["_location"]

    def save_to_file(self, target: PathLike | None = None) -> None:
        """
        Save Config to a location `path`.
        ----
        Args:
        - `path`: Path from where load the configuration. If `None`, Quack Pack will use location from which it loaded the configuration.
        """
        target = Path(target or self._location).expanduser()
        target.parent.mkdir(parents=True, exist_ok=True)
        data = self.model_dump(exclude_defaults=True, by_alias=True)
        with open(target.resolve(), "w") as f:
            yaml.dump(data, f)

    @staticmethod
    def load_from_file(path: PathLike | None = None) -> Config:
        """
        Factor Config class from a given `path`.
        ----
        Args:
        - `path`: Path from where load the configuration. If `None`, Quack Pack will use the default.
        ----
        Returns:
        - `Config`: Parsed configuration file with human interface.
        """
        path = Path(path or get_config_file()).expanduser()
        _logger.debug(f"Trying config @ {path}")
        try:
            with open(path.resolve()) as f:
                data = yaml.safe_load(f)
        except yaml.MarkedYAMLError as e:
            if (
                (mark := e.problem_mark)
                and hasattr(mark, "line")
                and hasattr(mark, "column")
                and isinstance(mark.line, int)
                and isinstance(mark.column, int)
            ):
                location = f"({mark.line + 1}:{mark.column + 1})"
            else:
                location = "(??:??)"
            message = f"Error in file {path} @ {location}: {e.problem}"
            # TODO: Move to OUI, when it's merged.
            # TODO: Display code snippet, like miette does https://docs.rs/miette/latest/miette/index.html#about.
            # TODO: Maybe don't `sys.exit(69)`?
            print(message, file=sys.stderr)
            sys.exit(69)
        except FileNotFoundError:
            data = {}
        data["_location"] = path
        return Config.model_validate(data)
