from pydantic import BaseModel, Field

from quackpack.config.project.models import BuildProfile, TargetProfile
from quackpack.util.default_pydantic_options import default_pydantic_options

__all__ = ["BuildEntry"]


class BuildEntry(BaseModel):
    """
    Class representing build entry in Quack Pack configuration.
    """

    targets: TargetProfile = Field(default_factory=TargetProfile)
    """
    Extra options for specified target.
    """
    profiles: BuildProfile = Field(default_factory=BuildProfile)
    """
    Extra options for specified profile.
    """
    model_config = default_pydantic_options()
