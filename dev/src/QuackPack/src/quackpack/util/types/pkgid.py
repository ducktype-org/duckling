from hashlib import sha256
from pathlib import Path
from typing import Any, Final, Literal, override
from urllib.parse import urlparse

from pydantic import (
    BaseModel,
    ConfigDict,
    Field,
    RootModel,
    field_serializer,
    field_validator,
)
from pydantic.config import ExtraValues

from quackpack.core.types.manifest.source import SourceKind
from quackpack.util.duckling_compatibility import is_valid_identifier
from quackpack.util.types.version import Version

_ALLOW_EXTRA_ARGS: Final[ExtraValues] = "forbid"


# NOTE(stach): `validate_*` jest niepotrzebne, bo `frozen=True`.
# NOTE(stach): `use_enum_values` jest niepotrzebne, bo storage nie używa enumów w tych modelach.
_PACKAGE_ID_MODEL_CONFIG = ConfigDict(extra=_ALLOW_EXTRA_ARGS, frozen=True)

# NOTE(stach): `validate_*` jest niepotrzebne, bo `frozen=True`.
_IDENTIFIER_CONFIG = ConfigDict(frozen=True)


class Identifier(RootModel[str]):
    """
    Valid Duckling identifier.
    """

    model_config = _IDENTIFIER_CONFIG

    @field_validator("root", mode="after")
    @classmethod
    def validate_root(cls, value: str) -> str:
        if not is_valid_identifier(value):
            raise ValueError(f"{value} is not a valid identifier")
        return value

    @override
    def __str__(self) -> str:
        return self.root

    @override
    def __eq__(self, other: object) -> bool:
        if isinstance(other, str):
            return self.root == other
        return super().__eq__(other)

    @override
    def __hash__(self) -> int:
        return hash(self.root)


class PackageId(BaseModel):
    inner: RegistryPackageId | GitPackageId | LocalPackageId = Field(
        ..., discriminator="type"
    )
    model_config = ConfigDict(frozen=True)

    def kind(self) -> SourceKind:
        return self.inner.kind()

    def is_registry(self) -> bool:
        return self.kind() == SourceKind.Registry

    def is_git(self) -> bool:
        return self.kind() == SourceKind.Git

    def is_local(self) -> bool:
        return self.kind() == SourceKind.Local

    def storage_name(self) -> str:
        return self.inner.storage_name()


class RegistryPackageId(BaseModel):
    type: Literal["registry"] = "registry"
    id: Identifier
    version: Version
    url: str
    model_config = _PACKAGE_ID_MODEL_CONFIG

    @field_validator("version", mode="before")
    @classmethod
    def _convert_version(cls, value: Any) -> Any:
        if not isinstance(value, str):
            return value
        return Version.create_from_string(value)

    def kind(self) -> SourceKind:
        return SourceKind.Registry

    def storage_name(self) -> str:
        parsed = urlparse(self.url)
        if parsed.hostname is None:
            raise ValueError("Invalid ducknest URL")
        return f"{self.type}-{parsed.hostname}-{self.id!s}-{self.version!s}"

    def upcast(self) -> PackageId:
        return PackageId(inner=self)

    @field_serializer("id", "version")
    def serialize_as_str(self, x: Any) -> str:
        return str(x)


class GitPackageId(BaseModel):
    type: Literal["git"] = "git"
    url: str
    commit: str
    model_config = _PACKAGE_ID_MODEL_CONFIG

    def kind(self) -> SourceKind:
        return SourceKind.Git

    def storage_name(self) -> str:
        hash = sha256()
        hash.update(bytes(self.url, encoding="utf-8"))
        return f"{self.type}-{hash.hexdigest()}-{self.commit}"

    def upcast(self) -> PackageId:
        return PackageId(inner=self)

    @field_validator("commit", mode="after")
    @classmethod
    def _validate_git_commit_hash(cls, value: str) -> str:
        try:
            int(value, 16)
        except ValueError:
            raise ValueError("Git hashes are valid hex numbers") from None
        if len(value) != 40:
            raise ValueError("Git hashes are 40 digits")
        return value


class LocalPackageId(BaseModel):
    type: Literal["local"] = "local"
    path: Path
    model_config = _PACKAGE_ID_MODEL_CONFIG

    def kind(self) -> SourceKind:
        return SourceKind.Local

    def storage_name(self) -> str:
        hash = sha256()
        hash.update(bytes(str(self.path), encoding="utf-8"))
        return f"{self.type}-{hash.hexdigest()}"

    def upcast(self) -> PackageId:
        return PackageId(inner=self)
