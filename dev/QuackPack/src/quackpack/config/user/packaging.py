from typing import Annotated

from pydantic import BaseModel, Field

from quackpack.util.default_pydantic_options import default_pydantic_options

__all__ = ["PackagingEntry"]


class PackagingEntry(BaseModel):
    """
    Class representing packaging entry in Quack Pack configuration.
    """

    # FIXME: Use StrictBool.
    build_from_source: Annotated[bool, Field(default=False)]
    """
    If `True`, then all downloaded packages will be compiled on the host machine.
    """
    model_config = default_pydantic_options()
    # TODO: Add dependency solver.
