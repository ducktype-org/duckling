from __future__ import annotations

from pathlib import Path
from typing import Literal

from pydantic import BaseModel, Field
from pydantic.types import StrictStr

from quackpack.config.project.models.build_profile import BuildProfile
from quackpack.config.project.models.dependencies import Dependencies
from quackpack.config.project.models.metadata import Metadata
from quackpack.config.project.models.target_profile import TargetProfile
from quackpack.util.default_pydantic_options import default_pydantic_options
from quackpack.util.deser.serialize import serialize


# TODO: Add rest of missing fields.
class Configuration(BaseModel):
    """
    Full project configuration.
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
    features: dict[StrictStr, list[StrictStr]] = {}
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

    model_config = default_pydantic_options()

    @classmethod
    def create_basic_with_name(cls, name: str) -> Configuration:
        return Configuration(metadata=Metadata.create_basic_with_name(name))

    def save_to(self, destination: str | Path, *, mode: Literal["w", "x"] = "w") -> None:
        data = self.model_dump(exclude_defaults=True)
        serialize(destination=destination, data=data, mode=mode)
