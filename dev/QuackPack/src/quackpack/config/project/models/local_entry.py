from pathlib import Path
from typing import Self

from pydantic import BaseModel, field_serializer, model_validator

from quackpack.util.default_pydantic_options import default_pydantic_options


class LocalEntry(BaseModel):
    """
    Local source of the dependency.
    """

    # NOTE: We use plain Path with after validator, so users can use `~`.
    path: Path
    """
    Local path to the dependency.
    """

    @field_serializer("path")
    def _serialize_as_str(self, value: Path) -> str:
        return str(value)

    model_config = default_pydantic_options()

    @model_validator(mode="after")
    def _check_if_path_is_dir(self) -> Self:
        if not self.path.expanduser().is_dir():
            raise ValueError(f"'{self.path}' is not a directory")
        return self
