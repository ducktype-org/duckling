from pydantic import BaseModel
from pydantic.types import StrictStr

from quackpack.util.default_pydantic_options import default_pydantic_options
from quackpack.util.pydantic_helpers import PydanticMutableDict


class BuildEntry(BaseModel):
    """
    Single entry for profile-specific options.
    """

    compiler_flags: list[StrictStr] = []
    """
    Additional compiler flags.
    """
    model_config = default_pydantic_options()


class BuildProfile(PydanticMutableDict[StrictStr, BuildEntry]):
    """
    Class representing extra profile options.
    """

    def get_options_for(self, profile: str) -> list[str]:
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
