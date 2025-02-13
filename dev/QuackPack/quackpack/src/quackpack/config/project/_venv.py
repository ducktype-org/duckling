from __future__ import annotations

from pathlib import Path
from typing import Final

import yaml

from common.venv_config import Configuration


# TODO: Should this really be called Venv and live here? Maybe name it "ProjectLoader", and add:
#       1. walking path upwards looking for CONFIG_FILE_NAME,
#       2. getting venv id from config,
#       3. returning some other venv object via getter? (Something which remembers real venv path on disk, with some extra metadata?)
class Venv:
    CONFIG_FILE_NAME: Final[str] = "quackconfig.yml"

    @classmethod
    def find_venv(cls) -> Venv | None:
        current_dir = Path.cwd()

        for parent in current_dir.parents:
            path = parent / cls.CONFIG_FILE_NAME
            if path.exists():
                return Venv(path)

        return None

    def __init__(self, path: Path) -> None:
        # TODO: venv relocation is disabled right now
        self.path: Final[Path] = path
        self.config_file_path: Final[Path] = self.path / self.CONFIG_FILE_NAME
        self.config: Configuration = self.read_config()

    def write_config(self) -> None:
        # TODO: some kind of error handling
        with open(self.config_file_path, "w") as config_file:
            config_dict = self.config.model_dump()
            yaml.dump(config_dict, config_file, sort_keys=False)

    def read_config(self) -> Configuration:
        if self.config_file_path.exists():
            with open(self.config_file_path) as config_file:
                config_dict = yaml.safe_load(config_file)
                return Configuration(**config_dict)
        else:
            return Configuration()

    # TODO: rest of methods
