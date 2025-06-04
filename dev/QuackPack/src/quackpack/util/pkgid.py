from __future__ import annotations

from hashlib import sha256
from pathlib import Path
from typing import Any, Literal, override

from pydantic import AnyUrl, BaseModel, ConfigDict, Field, RootModel, field_validator

from quackpack.util.default_pydantic_options import DEFAULT_MODEL_CONFIG, DEFAULT_ROOT_CONFIG
from quackpack.util.duckling_compatibility import is_valid_identifier
from quackpack.util.version import Version

_RESOLVED_ID_MODEL_CONFIG = DEFAULT_MODEL_CONFIG.copy()
_RESOLVED_ID_MODEL_CONFIG["frozen"] = True

_PKGID_CONFIG = DEFAULT_ROOT_CONFIG.copy()
_PKGID_CONFIG["frozen"] = True


# FIXME: Port more methods.
class Identifier(RootModel[str]):
    """
    Valid Duckling identifier.
    """

    model_config = _PKGID_CONFIG

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


class ResolvedId(BaseModel):
    inner: ResolvedIdDucknest | ResolvedIdGit | ResolvedIdLocal = Field(..., discriminator="type")
    model_config = ConfigDict(frozen=True)

    def dir_name(self) -> str:
        return self.inner.dir_name()


class ResolvedIdDucknest(BaseModel):
    type: Literal["ducknest"] = "ducknest"
    id: Identifier
    version: Version
    url: AnyUrl
    model_config = _RESOLVED_ID_MODEL_CONFIG

    @field_validator("version", mode="before")
    @classmethod
    def _convert_version(cls, value: Any) -> Any:
        if not isinstance(value, str):
            return value
        return Version.create_from_string(value)

    def dir_name(self) -> str:
        # TODO pydantic ogarnia url???
        # parsed = urlparse(self.url.host)
        # if parsed.hostname is None:
        #     raise ValueError("Invalid ducknest URL")
        return f"{self.type}-{self.url.host}-{self.id!s}-{self.version!s}"

    def upcast(self) -> ResolvedId:
        return ResolvedId(inner=self)


class ResolvedIdGit(BaseModel):
    type: Literal["git"] = "git"
    url: str
    commit: str
    model_config = _RESOLVED_ID_MODEL_CONFIG

    def dir_name(self) -> str:
        hash = sha256()
        hash.update(bytes(self.url, encoding="utf-8"))
        return f"{self.type}-{hash.hexdigest()}-{self.commit}"

    def upcast(self) -> ResolvedId:
        return ResolvedId(inner=self)

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


class ResolvedIdLocal(BaseModel):
    type: Literal["local"] = "local"
    path: Path
    model_config = _RESOLVED_ID_MODEL_CONFIG

    def dir_name(self) -> str:
        hash = sha256()
        hash.update(bytes(str(self.path), encoding="utf-8"))
        return f"{self.type}-{hash.hexdigest()}"

    def upcast(self) -> ResolvedId:
        return ResolvedId(inner=self)
