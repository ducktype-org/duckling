from typing import Annotated, Final

from pydantic import BaseModel, Field, PositiveInt

from quackpack.util.default_pydantic_options import default_pydantic_options

_DEFAULT_TYPO_TOLERANCE: Final[int] = 1

__all__ = ["Security", "TypoTolerance"]


class TypoTolerance(BaseModel):
    """
    Class representing Typo tolerance entry in Quack Pack configuration.
    """

    # FIXME: Use StrictBool.
    enabled: Annotated[bool, Field(default=False)]
    """
    If `True`, then Quack Pack CLI will update main commands.
    """
    max_distance: Annotated[PositiveInt, Field(default=_DEFAULT_TYPO_TOLERANCE)]
    """
    Radius of maximum disk in Levenshtein distance of possible matches.

    If there is more than one match, then Quack Pack will not try to guess.
    """
    model_config = default_pydantic_options()


class Security(BaseModel):
    """
    Class representing security entry in Quack Pack configuration.
    """

    # They are callable, because Python is great language, so let's just silence errors.
    typo_tolerance: Annotated[TypoTolerance, Field(default_factory=TypoTolerance)]  # pyright: ignore[reportArgumentType]
    """
    TypoTolerance configuration.
    """
    model_config = default_pydantic_options()
