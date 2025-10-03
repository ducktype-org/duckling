from abc import ABC, abstractmethod
from dataclasses import dataclass
from pathlib import Path
from typing import override

from quackpack.core.types.manifest.source import SourceKind
from quackpack.util.types.pkgid import Identifier


class ResolvedId(ABC):
    def is_registry(self) -> bool:
        return self.kind() is SourceKind.Registry

    def is_git(self) -> bool:
        return self.kind() is SourceKind.Git

    def is_local(self) -> bool:
        return self.kind() is SourceKind.Local

    @abstractmethod
    def kind(self) -> SourceKind: ...


@dataclass(frozen=True)
class ResolvedIdRegistry(ResolvedId):
    registry_url: str
    package_name: Identifier

    @override
    def kind(self) -> SourceKind:
        return SourceKind.Registry

    @override
    def __eq__(self, other: object) -> bool:
        return (
            isinstance(other, ResolvedIdRegistry)
            and self.registry_url == other.registry_url
            and self.package_name == other.package_name
        )

    @override
    def __hash__(self) -> int:
        return hash((self.registry_url, self.package_name))


@dataclass(frozen=True)
class ResolvedIdGit(ResolvedId):
    repository_url: str
    commit: str

    @override
    def kind(self) -> SourceKind:
        return SourceKind.Git

    @override
    def __eq__(self, other: object) -> bool:
        return (
            isinstance(other, ResolvedIdGit)
            and self.repository_url == other.repository_url
            and self.commit == other.commit
        )

    @override
    def __hash__(self) -> int:
        return hash((self.repository_url, self.commit))


@dataclass
class ResolvedIdLocal(ResolvedId):
    local_path: Path

    @override
    def kind(self) -> SourceKind:
        return SourceKind.Local

    @override
    def __eq__(self, other: object) -> bool:
        return isinstance(other, ResolvedIdLocal) and self.local_path == other.local_path

    @override
    def __hash__(self) -> int:
        return hash(self.local_path)
