from typing import Final

from pydantic import ConfigDict
from pydantic.config import ExtraValues

_ALLOW_EXTRA_ARGS: Final[ExtraValues] = "forbid"


DEFAULT_MODEL_CONFIG = ConfigDict(
    extra=_ALLOW_EXTRA_ARGS,
    validate_assignment=True,
    validate_return=True,
    validate_default=True,
    use_enum_values=True,
)

DEFAULT_ROOT_CONFIG = DEFAULT_MODEL_CONFIG.copy()
# RootModel's `model_config` can't have `"extra"` field set.
del DEFAULT_ROOT_CONFIG["extra"]
