from __future__ import annotations

from itertools import chain
from pathlib import Path
from typing import Final

import yaml

from common.venv_config import Configuration
from quackpack.config.strict_yaml_parsing import load_and_validate


# TODO: Should this really be called Venv and live here? Maybe name it "ProjectLoader", and add:
#       1. walking path upwards looking for CONFIG_FILE_NAME,
#       2. getting venv id from config,
#       3. returning some other venv object via getter? (Something which remembers real venv path on disk, with some extra metadata?)
class Venv:
    CONFIG_FILE_NAME: Final[str] = "quackconfig.yml"

    @classmethod
    def find_venv(cls) -> Venv | None:
        current_dir = Path.cwd()

        for potential_location in chain((current_dir,), current_dir.parents):
            path = potential_location / cls.CONFIG_FILE_NAME
            if path.is_file():
                return Venv(path)

        return None

    def __init__(self, path: Path) -> None:
        # TODO: venv relocation is disabled right now
        self.path: Final[Path] = path
        self.config_file_path: Final[Path] = self.path
        self.config: Configuration = self.read_config()

    def write_config(self) -> None:
        # TODO: some kind of error handling
        with open(self.config_file_path, "w") as config_file:
            config_dict = self.config.model_dump()
            yaml.dump(config_dict, config_file, sort_keys=False)

    def read_config(self) -> Configuration:
        return load_and_validate(self.config_file_path, Configuration)

    # TODO: rest of methods
