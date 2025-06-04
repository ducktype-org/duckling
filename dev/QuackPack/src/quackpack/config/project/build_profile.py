from pydantic import BaseModel, StrictStr

from quackpack.util.default_pydantic_options import DEFAULT_MODEL_CONFIG
from quackpack.util.pkgid import Identifier
from quackpack.util.pydantic_helpers import PydanticMutableDict


class BuildEntry(BaseModel):
    """
    Single entry for profile-specific options.
    """

    compiler_flags: list[StrictStr] = []
    """
    Additional compiler flags.
    """

    model_config = DEFAULT_MODEL_CONFIG


class BuildProfile(PydanticMutableDict[Identifier, BuildEntry]):
    """
    Class representing extra profile options.
    """

    def get_options_for(self, profile: Identifier) -> list[str]:
        """
        Get additional compiler flags for profile `profile`.
        ----
        Args:
        - `profile`: profile to be matched against.
        ----
        Returns:
        - `list[str]`: additional compiler flags.
        """
        if profile in self:
            return self[profile].compiler_flags
        return []
