from __future__ import annotations

from itertools import chain
from pathlib import Path
from typing import Final

from quackpack.config.project.models import Configuration
from quackpack.util.deser.deserialize import deserialize
from quackpack.util.deser.errors import ConfigFileLoadError
from quackpack.util.errors import QuackPackError
from quackpack.util.logger import get_logger

logger = get_logger(__name__)


# TODO: Should this really be called Venv and live here? Maybe name it "ProjectLoader", and add:
#       1. walking path upwards looking for CONFIG_FILE_NAME,
#       2. getting venv id from config,
#       3. returning some other venv object via getter? (Something which remembers real venv path on disk, with some extra metadata?)
class Venv:
    CONFIG_FILE_NAME: Final[str] = "quackconfig.yml"

    @classmethod
    def find_from(cls, start_path: Path) -> Venv:
        start_path = start_path.expanduser()
        if not start_path.is_dir():
            raise NotADirectoryError(start_path)
        logger.debug(f"Starting looking for Venv from {start_path}")

        start_path = start_path.resolve(strict=True)
        for potential_location in chain((start_path,), start_path.parents):
            path = potential_location / cls.CONFIG_FILE_NAME
            if path.is_file():
                logger.debug(f"Found Venv at {path}")
                return Venv(path)

        raise QuackPackError(f"No Venv found from {start_path} to {start_path.root}")

    def __init__(self, path: Path) -> None:
        self.config_file_path: Final[Path] = path
        self.config: Configuration = self.read_config()

    def write_config(self) -> None:
        # TODO: some kind of error handling
        logger.debug(f"Saving Venv configuration to {self.config_file_path}")
        self.config.save_to(self.config_file_path)

    def read_config(self) -> Configuration:
        logger.debug(f"Reading Venv configuration from {self.config_file_path}")
        try:
            return deserialize(self.config_file_path, Configuration)
        except ConfigFileLoadError as e:
            raise QuackPackError(e) from e
