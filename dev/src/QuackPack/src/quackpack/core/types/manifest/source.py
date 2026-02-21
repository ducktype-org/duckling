from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass
from enum import Enum, auto
from pathlib import Path
from typing import cast, override

from quackpack.core.types.manifest.schemas.registry import (
    DefaultSourceSchema,
    GitSourceSchema,
    LocalSourceSchema,
    RegistrySourceSchema,
)
from quackpack.util.logger import get_logger
from quackpack.util.types.errors import QuackPackError

logger = get_logger(__name__)


class SourceKind(Enum):
    Registry = auto()
    Git = auto()
    Local = auto()


class Source(ABC):
    def is_registry(self) -> bool:
        return self.kind() is SourceKind.Registry

    def is_git(self) -> bool:
        return self.kind() is SourceKind.Git

    def is_local(self) -> bool:
        return self.kind() is SourceKind.Local

    def as_registry(self) -> RegistrySource:
        if not self.is_registry():
            raise QuackPackError("not a registry source")
        return self.as_registry_unchecked()

    def as_git(self) -> GitSource:
        if not self.is_git():
            raise QuackPackError("not a git source")
        return self.as_git_unchecked()

    def as_local(self) -> LocalSource:
        if not self.is_local():
            raise QuackPackError("not a local source")
        return self.as_local_unchecked()

    def as_registry_unchecked(self) -> RegistrySource:
        return cast(RegistrySource, self)

    def as_git_unchecked(self) -> GitSource:
        return cast(GitSource, self)

    def as_local_unchecked(self) -> LocalSource:
        return cast(LocalSource, self)

    @abstractmethod
    def into_schema(self) -> RegistrySourceSchema: ...

    @abstractmethod
    def kind(self) -> SourceKind: ...

    @classmethod
    def from_schema(cls, schema: RegistrySourceSchema) -> Source:
        if isinstance(schema.inner, DefaultSourceSchema):
            return RegistrySource(registry_url=schema.inner.registry_url)
        if isinstance(schema.inner, GitSourceSchema):
            return GitSource(
                git_url=schema.inner.git_url,
                commit=schema.inner.commit,
                tag=schema.inner.tag,
                branch=schema.inner.branch,
            )
        if isinstance(
            schema.inner, LocalSourceSchema
        ):  # pyright: ignore[reportUnnecessaryIsInstance]
            return LocalSource(
                absolute_dir_root=schema.inner.absolute_dir_root,
                dir_entry_in_manifest=schema.inner.dir_entry_in_manifest,
            )
        raise QuackPackError("unknown registry schema source")


@dataclass(frozen=True, kw_only=True)
class RegistrySource(Source):
    registry_url: str

    @override
    def kind(self) -> SourceKind:
        return SourceKind.Registry

    @override
    def into_schema(self) -> RegistrySourceSchema:
        return RegistrySourceSchema(
            inner=DefaultSourceSchema(registry_url=self.registry_url)
        )


@dataclass(frozen=True, kw_only=True)
class GitSource(Source):
    git_url: str
    branch: str | None = None
    tag: str | None = None
    commit: str | None = None

    @override
    def kind(self) -> SourceKind:
        return SourceKind.Git

    @override
    def into_schema(self) -> RegistrySourceSchema:
        return RegistrySourceSchema(
            inner=GitSourceSchema(
                git_url=self.git_url,
                commit=self.commit,
                tag=self.tag,
                branch=self.branch,
            )
        )


@dataclass(frozen=True, kw_only=True)
class LocalSource(Source):
    absolute_dir_root: Path
    dir_entry_in_manifest: Path

    @override
    def kind(self) -> SourceKind:
        return SourceKind.Local

    @override
    def into_schema(self) -> RegistrySourceSchema:
        return RegistrySourceSchema(
            inner=LocalSourceSchema(
                absolute_dir_root=self.absolute_dir_root,
                dir_entry_in_manifest=self.dir_entry_in_manifest,
            )
        )
