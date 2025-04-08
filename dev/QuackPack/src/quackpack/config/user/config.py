from __future__ import annotations

from os import PathLike
from pathlib import Path
from typing import Annotated

from pydantic import BaseModel, Field, FilePath, PrivateAttr, StrictStr

from quackpack.config.user.build import BuildEntry
from quackpack.config.user.cache import CacheLocationEntry
from quackpack.config.user.location import config_filepath
from quackpack.config.user.packaging import PackagingEntry
from quackpack.config.user.repository import RepositoryEntry
from quackpack.config.user.security import Security
from quackpack.util.console import Console
from quackpack.util.default_pydantic_options import default_pydantic_options
from quackpack.util.deser.deserialize import deserialize
from quackpack.util.deser.errors import ConfigFileLoadError
from quackpack.util.deser.serialize import serialize
from quackpack.util.errors import QuackPackError
from quackpack.util.logger import get_logger

logger = get_logger(__name__)


__all__ = ["Config"]


# FIXME: Add target-dependent compilation flags.
class Config(BaseModel):
    """
    Class representing global Quack Pack configuration.
    """

    # They are callable, because Python is great language, so let's just silence errors.
    aliases: Annotated[dict[StrictStr, StrictStr | list[StrictStr]], Field(default={})]  # pyright: ignore[reportUnknownArgumentType]
    """
    Any user-defined CLI aliases.

    Note that `str` values will be `split()`-ed by spaces.
    """
    cache: Annotated[CacheLocationEntry, Field(default_factory=CacheLocationEntry)]  # pyright: ignore[reportArgumentType]
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
    _location: FilePath = PrivateAttr()
    model_config = default_pydantic_options()

    def save_to_file(self, target: PathLike[str] | None = None) -> None:
        """
        Save Config to a location `path`.
        ----
        Args:
        - `path`: Path from where load the configuration. If `None`, Quack Pack will use location from which it loaded the configuration.
        """
        target = Path(target or self._location).expanduser()
        target.parent.mkdir(parents=True, exist_ok=True)
        data = self.model_dump(exclude_defaults=True, by_alias=True)
        logger.debug(f"Saving user configuration {data} to {target.resolve()}")
        serialize(destination=target, data=data)

    @classmethod
    def load_from_file(cls, console: Console, path: PathLike[str] | None = None) -> Config:
        """
        Factor Config class from a given `path`.
        ----
        Args:
        - `console`: Console used for printing.
        - `path`: Path from where load the configuration. If `None`, Quack Pack will use the default.
        ----
        Returns:
        - `Config`: Parsed configuration file with human interface.
        ----
        Raises:
        - `QuackPackError`: if any error occurred.
        """
        path = Path(path or config_filepath()).expanduser()
        logger.debug(f"Looking for user configuration at {path}")
        if not path.is_absolute():
            console.warn(f"User configuration path '{path}' is not absolute")
        try:
            config = deserialize(path, Config)
            logger.debug(f"Found user configuration {config}")
        except FileNotFoundError:
            config = Config()  # pyright: ignore[reportCallIssue], these are handled by default values in `Field`.
            logger.debug("User configuration not found, falling back to defaults...")
        except ConfigFileLoadError as e:
            raise QuackPackError(e) from e
        config._location = path
        return config
