from typing import Annotated

from pydantic import BaseModel, Field, StrictStr

from quackpack.util.default_pydantic_options import default_pydantic_options

__all__ = ["RepositoryEntry"]


class RepositoryEntry(BaseModel):
    """
    Class representing repository entry in Quack Pack configuration.
    """

    default: Annotated[StrictStr | list[StrictStr], Field(default=["TODO: Add here Ducknest URL."])]
    """
    Default Ducknest URL or list with Ducknest's URLs.
    """
    extra: Annotated[StrictStr | list[StrictStr] | None, Field(default=None)]
    """
    Additional locations of Quack Pack packages servers.
    """
    model_config = default_pydantic_options()
