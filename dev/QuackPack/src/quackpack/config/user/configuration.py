from __future__ import annotations

from os import PathLike, getenv
from pathlib import Path

from pydantic import BaseModel, Field, FilePath, PrivateAttr, StrictStr

from quackpack.config.user.build import BuildEntry
from quackpack.config.user.cache import Cache
from quackpack.config.user.packaging import PackagingEntry
from quackpack.config.user.repository import RepositoryEntry
from quackpack.config.user.security import Security
from quackpack.config.user.storage import StorageEntry
from quackpack.util.console import Console
from quackpack.util.default_pydantic_options import DEFAULT_MODEL_CONFIG
from quackpack.util.deser.deserialize import deserialize
from quackpack.util.deser.errors import ConfigFileLoadError
from quackpack.util.errors import QuackPackError
from quackpack.util.logger import get_logger
from quackpack.util.xdg_directories import config_directory

from ._relative_files_loc import QUACKPACK_CONFIG_LOCATION

logger = get_logger(__name__)


def _config_file_directory() -> Path:
    """
    Get directory path suitable for the configuration file.
    ----
    Returns:
    - `pathlib.Path`: Path to directory of the configuration file.
    """
    if directory := getenv("QP_CONFIG"):
        return Path(directory)
    return config_directory() / QUACKPACK_CONFIG_LOCATION


def config_filepath() -> Path:
    """
    Get path of the configuration file.
    ----
    Returns:
    - `pathlib.Path`: Path to the configuration file.
    """
    return _config_file_directory() / "config.yaml"


__all__ = ["Configuration"]


class Configuration(BaseModel):
    """
    Class representing global Quack Pack configuration.
    """

    aliases: dict[StrictStr, StrictStr | list[StrictStr]] = {}
    """
    Any user-defined CLI aliases.

    Note that `str` values will be `split()`-ed by spaces.
    """

    cache: Cache = Field(default_factory=Cache)
    """
    Quack Pack cache settings.
    """

    packaging: PackagingEntry = Field(default_factory=PackagingEntry)
    """
    Quack Pack packages settings.
    """

    build: BuildEntry = Field(default_factory=BuildEntry)
    """
    Settings used for configuring packages built from source.
    """

    repository: RepositoryEntry = Field(default_factory=RepositoryEntry)
    """
    Settings for managing Quack Pack packages servers.
    """

    storage: StorageEntry = Field(default_factory=StorageEntry)
    """
    Settings for managing package and virtual enviroment storage.
    """

    security: Security = Field(default_factory=Security)
    """
    Any potentially unsecure settings.
    Keep sane defaults.
    """

    _location: FilePath = PrivateAttr()

    model_config = DEFAULT_MODEL_CONFIG

    @classmethod
    def load_from_file(cls, console: Console, path: PathLike[str] | None = None) -> Configuration:
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
            config = deserialize(path, Configuration)
            logger.debug(f"Found user configuration {config}")
        except FileNotFoundError:
            config = Configuration()
            logger.debug("User configuration not found, falling back to defaults...")
        except ConfigFileLoadError as e:
            raise QuackPackError(e) from e
        config._location = path
        return config
