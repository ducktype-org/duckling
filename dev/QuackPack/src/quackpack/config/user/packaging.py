from pydantic import BaseModel

from quackpack.util.default_pydantic_options import DEFAULT_MODEL_CONFIG

__all__ = ["PackagingEntry"]


class PackagingEntry(BaseModel):
    """
    Class representing packaging entry in Quack Pack configuration.
    """

    # FIXME: Use StrictBool.
    build_from_source: bool = False
    """
    If `True`, then all downloaded packages will be compiled on the host machine.
    """

    model_config = DEFAULT_MODEL_CONFIG
    # TODO: Add dependency solver.
