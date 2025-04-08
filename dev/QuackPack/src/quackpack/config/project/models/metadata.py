from __future__ import annotations

from typing import Any, Self

from pydantic import BaseModel, StrictStr, model_validator

from quackpack.util.default_pydantic_options import default_pydantic_options
from quackpack.util.validators import convert_to_version
from quackpack.util.version import Version


# TODO: in this class add source of the package
class Metadata(BaseModel):
    """
    Global project metadata.
    """

    author: StrictStr | None = None
    """
    Author of the project.
    """
    version: Version
    """
    Version of the project.
    """
    id: StrictStr  # TODO: Change to validated (different) type.
    """
    Internal project ID.
    """
    name: StrictStr
    """
    Name of the project.
    """
    license: StrictStr | None = None
    """
    License used in the project.
    """
    description: StrictStr | None = None
    """
    Description of the project.
    """
    # FIXME: Use StrictBool.
    is_temporary: bool = (
        False  # TODO: If we want to use different name in configuration file, then alias it here.
    )
    """
    If `True`, then this project uses temporary venv.
    """
    compiler_version: Version | None = None
    """
    Version of compiler required for that project.
    """
    vm_version: Version | None = None
    """
    Version of VM required for that project.
    """

    model_config = default_pydantic_options()

    @model_validator(mode="before")
    @classmethod
    def _convert_version(cls, values: Any) -> Any:
        for member in ("version", "compiler_version", "vm_version"):
            values = convert_to_version(values, member)
        return values

    @classmethod
    def create_basic_with_name(cls, name: str) -> Metadata:
        return Metadata(name=name, version=Version.default(), id=f"FIXME: Add real id {name}")

    @classmethod
    def is_valid_project_name(cls, name: str) -> bool:
        def is_valid_identifier(x: str) -> bool:
            return x.isalnum() or x == "_"

        return bool(name) and name[0].isalnum() and all(is_valid_identifier(x) for x in name)

    @model_validator(mode="after")
    def check_project_name(self) -> Self:
        if not Metadata.is_valid_project_name(self.name):
            raise ValueError(f"'{self.name}' is not valid Venv name")
        return self
