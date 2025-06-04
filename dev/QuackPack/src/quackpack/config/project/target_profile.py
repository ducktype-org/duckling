from pydantic import BaseModel
from pydantic.types import StrictStr

from quackpack.util.default_pydantic_options import DEFAULT_MODEL_CONFIG
from quackpack.util.pkgid import Identifier
from quackpack.util.pydantic_helpers import PydanticMutableDict


class TargetEntry(BaseModel):
    """
    Single entry for target-specific options.
    """

    compiler_flags: list[StrictStr] = []
    """
    Additional compiler flags.
    """

    model_config = DEFAULT_MODEL_CONFIG


# FIXME: Check valid targets?
class TargetProfile(PydanticMutableDict[Identifier, TargetEntry]):
    """
    Class representing extra target options.
    """

    def get_options_for(self, target: Identifier) -> list[str]:
        """
        Get additional compiler flags for target `target`.
        ----
        Args:
        - `target`: target to be matched against.
        ----
        Returns:
        - `list[str]`: additional compiler flags.
        """
        if target in self:
            return self[target].compiler_flags
        return []
