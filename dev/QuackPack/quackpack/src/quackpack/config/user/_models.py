# pyright: standard
from __future__ import annotations

import logging
import os
from enum import StrEnum
from pathlib import Path
from typing import Annotated, Any, Final

import yaml
from pydantic import BaseModel, ConfigDict, Field, PrivateAttr, field_serializer, model_validator
from pydantic.alias_generators import to_pascal
from pydantic.config import ExtraValues

from quackpack.config.strict_yaml_parsing import load_and_validate
from quackpack.errors import QuackPackError
from quackpack.logger import get_logger

from ._location import config_filepath


def logger() -> logging.Logger:
    """
    Get `logging.Logger` for this module.
    """
    return get_logger(__name__)


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
        Try to parse a string as a `Size`.
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
__ALLOW_EXTRA_ARGS: Final[ExtraValues] = "forbid"


def _default_model_config():
    return ConfigDict(extra=__ALLOW_EXTRA_ARGS, alias_generator=to_pascal)


def _default_cache_directory() -> Path:
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

    path: Annotated[Path, Field(default=_default_cache_directory())]
    """
    Path to the Quack Pack cache directory.
    """
    max_size: Annotated[Size, Field(default=__DEFAULT_MAX_CACHE_SIZE)]
    """
    Maximum available size of Quack Pack cache directory.
    """
    model_config = _default_model_config()

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
    """
    If `True`, then all downloaded packages will be compiled on the host machine.
    """
    model_config = _default_model_config()
    # TODO: Add dependency solver.


class BuildEntry(BaseModel):
    """
    Class representing build entry in Quack Pack configuration.
    """

    extra_flags: Annotated[str, Field(default="")]
    """
    String with any extra flags, which will be passed down to the compiler.
    """
    model_config = _default_model_config()


class RepositoryEntry(BaseModel):
    """
    Class representing repository entry in Quack Pack configuration.
    """

    default: Annotated[str | list[str] | None, Field(default=["TODO: Add here Ducknest URL."])]
    """
    If not `None`, then override default Quack Pack server URL.
    """
    extra: Annotated[str | list[str] | None, Field(default=None)]
    """
    Additional locations of Quack Pack packages servers.
    """
    model_config = _default_model_config()


class TypoTolerance(BaseModel):
    """
    Class representing Typo tolerance entry in Quack Pack configuration.
    """

    enabled: Annotated[bool, Field(default=False)]
    """
    If `True`, then Quack Pack CLI will update main commands.
    """
    max_distance: Annotated[int, Field(default=__DEFAULT_TYPO_TOLERANCE)]
    """
    Radius of maximum disk in Levenshtein distance of possible matches.

    If there is more than one match, then Quack Pack will not try to guess.
    """
    model_config = _default_model_config()


class Security(BaseModel):
    """
    Class representing security entry in Quack Pack configuration.
    """

    # They are callable, because Python is great language, so let's just silence errors.
    typo_tolerance: Annotated[TypoTolerance, Field(default_factory=TypoTolerance)]  # pyright: ignore[reportArgumentType]
    """
    TypoTolerance configuration.
    """
    model_config = _default_model_config()


class Config(BaseModel):
    """
    Class representing global Quack Pack configuration.
    """

    # They are callable, because Python is great language, so let's just silence errors.
    aliases: Annotated[dict[str, str | list[str]], Field(default={})]
    """
    Any user-defined CLI aliases.

    Note that `str` values will be `split()`-ed by spaces.
    """
    cache: Annotated[CacheEntry, Field(default_factory=CacheEntry)]  # pyright: ignore[reportArgumentType]
    """
    Quack Pack cache settings.
    """
    packaging: Annotated[PackagingEntry, Field(default_factory=PackagingEntry)]  # pyright: ignore[reportArgumentType]
    """
    Quack Pack packages settings.
    """
    build: Annotated[BuildEntry, Field(default_factory=BuildEntry)]  # pyright: ignore[reportArgumentType]
    """
    Settings used for configuring packages built from source.
    """
    repository: Annotated[RepositoryEntry, Field(default_factory=RepositoryEntry)]  # pyright: ignore[reportArgumentType]
    """
    Settings for managing Quack Pack packages servers.
    """
    security: Annotated[Security, Field(default_factory=Security)]  # pyright: ignore[reportArgumentType]
    """
    Any potentially unsecure settings.
    Keep sane defaults.
    """
    _location: Path = PrivateAttr()
    model_config = _default_model_config()

    def save_to_file(self, target: PathLike | None = None) -> None:
        """
        Save Config to a location `path`.
        ----
        Args:
        - `path`: Path from where load the configuration. If `None`, Quack Pack will use location from which it loaded the configuration.
        """
        target = Path(target or self._location).expanduser()
        target.parent.mkdir(parents=True, exist_ok=True)
        data = self.model_dump(exclude_defaults=False, by_alias=True)
        logger().debug(f"Saving user configuration {data} to {target.resolve()}")
        with open(target.resolve(), "w") as f:
            yaml.safe_dump(data, f)

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
        ----
        Raises:
        - `QuackPackError`: if any error occurred.
        """
        path = Path(path or config_filepath()).expanduser()
        logger().debug(f"Looking for user configuration @ {path}")
        try:
            config = load_and_validate(path, Config)
            logger().debug(f"Found user configuration {config}")
        except FileNotFoundError:
            config = Config()  # pyright: ignore[reportCallIssue], these are handled by default values in `Field`.
            logger().debug("User configuration not found, falling back to defaults...")
        except Exception as e:
            raise QuackPackError(e) from e
        config._location = path
        return config
