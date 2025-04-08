from typing import Final

from pydantic import ConfigDict
from pydantic.config import ExtraValues

_ALLOW_EXTRA_ARGS: Final[ExtraValues] = "forbid"


# FIXME: validate_default doesn't work because of CacheLocationEntry.
def default_pydantic_options():
    return ConfigDict(extra=_ALLOW_EXTRA_ARGS, validate_assignment=True, validate_return=True)
