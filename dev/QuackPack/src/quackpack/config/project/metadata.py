from __future__ import annotations

from typing import Any

from pydantic import BaseModel, StrictStr, field_validator

from quackpack.util.default_pydantic_options import DEFAULT_MODEL_CONFIG
from quackpack.util.pkgid import Identifier
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

    name: Identifier
    """
    Name of the project, used as identifier in code.
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

    model_config = DEFAULT_MODEL_CONFIG

    @field_validator("version", "compiler_version", "vm_version", mode="before")
    @classmethod
    def _convert_version(cls, value: Any) -> Any:
        if not isinstance(value, str):
            return value
        return Version.create_from_string(value)

    @classmethod
    def create_basic_with_name(cls, name: Identifier) -> Metadata:
        return Metadata(name=name, version=Version.default())
