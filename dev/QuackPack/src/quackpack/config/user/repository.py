from pydantic import BaseModel, Field, StrictStr

from quackpack.util.default_pydantic_options import DEFAULT_MODEL_CONFIG

__all__ = ["RepositoryEntry"]


class RepositoryEntry(BaseModel):
    """
    Class representing repository entry in Quack Pack configuration.
    """

    url: StrictStr = Field(default="http://localhost:9001")
    """
    Default Ducknest URL or list with Ducknest's URLs.
    """

    model_config = DEFAULT_MODEL_CONFIG
