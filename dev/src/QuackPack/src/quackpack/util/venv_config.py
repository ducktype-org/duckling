from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from quackpack.util.toml_config import TOMLConfig


@dataclass(kw_only=True, frozen=True)
class VenvConfig:
    config: TOMLConfig

    @classmethod
    def from_path_or_default(cls, path: Path | None) -> VenvConfig:
        if path:
            return VenvConfig(config=TOMLConfig.create_from_filepath(path))
        else:
            return VenvConfig(config=TOMLConfig.default())

    def is_ephemeral(self) -> bool:
        return self.config.get_bool("ephemeral") or False

    def storage_path(self) -> Path | None:
        return self.config.get_path("local_storage") or None

    def freezefile_exposed(self) -> bool:
        return self.config.get_bool("expose_freezefile") or False

    def set_freezefile_exposed(self, value: bool):
        self.config.set_bool("expose_freezefile", value)
