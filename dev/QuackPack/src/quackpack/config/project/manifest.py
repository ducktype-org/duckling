from __future__ import annotations

from pathlib import Path
from typing import Literal

from pydantic import BaseModel, Field

from quackpack.config.project.build_profile import BuildProfile
from quackpack.config.project.dependencies import Dependencies
from quackpack.config.project.metadata import Metadata
from quackpack.config.project.target_profile import TargetProfile
from quackpack.util.default_pydantic_options import DEFAULT_MODEL_CONFIG
from quackpack.util.deser.serialize import serialize
from quackpack.util.pkgid import Identifier


# TODO: Add rest of missing fields.
class Manifest(BaseModel):
    """
    Full project manifest.
    """

    metadata: Metadata
    """
    Project metadata.
    """

    dependencies: Dependencies = Field(default_factory=Dependencies)
    """
    Project dependencies.
    """

    dev_dependencies: Dependencies = Field(default_factory=Dependencies)
    """
    Project dev dependencies.
    """

    features: dict[Identifier, list[Identifier]] = {}
    """
    Project exposed feature flags.
    """

    profiles: BuildProfile = Field(default_factory=BuildProfile)
    """
    Extra options for different build profiles.
    """

    targets: TargetProfile = Field(default_factory=TargetProfile)
    """
    Extra options for different targets.
    """

    model_config = DEFAULT_MODEL_CONFIG

    @classmethod
    def create_basic_with_name(cls, name: Identifier) -> Manifest:
        return Manifest(metadata=Metadata.create_basic_with_name(name))

    def save_to(self, destination: str | Path, *, mode: Literal["w", "x"] = "w") -> None:
        data = self.model_dump(exclude_defaults=True)
        serialize(destination=destination, data=data, mode=mode)
