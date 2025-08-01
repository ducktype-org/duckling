from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from pathlib import Path
from typing import cast, override

from quackpack.manifest.dependency import Dependency
from quackpack.manifest.source import SourceKind
from quackpack.solver.types.resolved_id import ResolvedId, ResolvedIdGit, ResolvedIdLocal, ResolvedIdRegistry
from quackpack.util.types.pkgid import Identifier


class UnresolvedId(ABC):
    def is_registry(self) -> bool:
        return self.kind() is SourceKind.Registry

    def is_git(self) -> bool:
        return self.kind() is SourceKind.Git

    def is_local(self) -> bool:
        return self.kind() is SourceKind.Local

    @abstractmethod
    def kind(self) -> SourceKind: ...

    @abstractmethod
    def resolve(self, resolvents: IdResolvents) -> ResolvedId: ...

    @staticmethod
    def from_dependency(dependency: Dependency) -> UnresolvedId:
        match dependency.source.kind():
            case SourceKind.Registry:
                return UnresolvedIdRegistry.from_dependency(dependency)
            case SourceKind.Git:
                return UnresolvedIdGit.from_dependency(dependency)
            case SourceKind.Local:
                return UnresolvedIdLocal.from_dependency(dependency)


@dataclass(frozen=True)
class UnresolvedIdRegistry(UnresolvedId):
    registry_url: str
    package_name: Identifier

    @override
    def kind(self) -> SourceKind:
        return SourceKind.Registry

    @override
    def resolve(self, resolvents: IdResolvents) -> ResolvedIdRegistry:
        return resolvents.registry_resolvents[self]

    @staticmethod
    @override
    def from_dependency(dependency: Dependency) -> UnresolvedIdRegistry:
        assert dependency.spec.source.is_registry()
        url = dependency.spec.source.as_registry().registry_url
        assert url is not None
        return UnresolvedIdRegistry(registry_url=url, package_name=dependency.real_name)

    @override
    def __eq__(self, other: object) -> bool:
        return (
            isinstance(other, UnresolvedIdRegistry)
            and self.registry_url == other.registry_url
            and self.package_name == other.package_name
        )

    @override
    def __hash__(self) -> int:
        return hash((self.registry_url, self.package_name))


@dataclass(frozen=True)
class UnresolvedIdGit(UnresolvedId):
    repository_url: str
    branch: str | None = None
    tag: str | None = None
    commit: str | None = None

    @override
    def kind(self) -> SourceKind:
        return SourceKind.Git

    @override
    def resolve(self, resolvents: IdResolvents) -> ResolvedIdGit:
        return resolvents.git_resolvents[self]

    @staticmethod
    @override
    def from_dependency(dependency: Dependency) -> UnresolvedIdGit:
        assert dependency.spec.source.is_git()
        return UnresolvedIdGit(
            repository_url=dependency.spec.source.as_git().git_url,
            branch=dependency.spec.source.as_git().branch,
            tag=dependency.spec.source.as_git().tag,
            commit=dependency.spec.source.as_git().commit,
        )

    @override
    def __eq__(self, other: object) -> bool:
        return (
            isinstance(other, UnresolvedIdGit)
            and self.repository_url == other.repository_url
            and self.branch == other.branch
            and self.tag == other.tag
            and self.commit == other.commit
        )

    @override
    def __hash__(self) -> int:
        return hash((self.repository_url, self.branch, self.tag, self.commit))


@dataclass
class UnresolvedIdLocal(UnresolvedId):
    local_path: Path

    @override
    def kind(self) -> SourceKind:
        return SourceKind.Local

    @override
    def resolve(self, resolvents: IdResolvents) -> ResolvedIdLocal:
        return resolvents.local_resolvents[self]

    @staticmethod
    @override
    def from_dependency(dependency: Dependency) -> UnresolvedIdLocal:
        assert dependency.spec.source.is_local()
        return UnresolvedIdLocal(local_path=dependency.spec.source.as_local().absolute_dir_root)

    @override
    def __eq__(self, other: object) -> bool:
        return isinstance(other, UnresolvedIdLocal) and self.local_path == other.local_path

    @override
    def __hash__(self) -> int:
        return hash(self.local_path)


@dataclass
class IdResolvents:
    registry_resolvents: dict[UnresolvedIdRegistry, ResolvedIdRegistry] = field(
        default_factory=dict[UnresolvedIdRegistry, ResolvedIdRegistry]
    )
    git_resolvents: dict[UnresolvedIdGit, ResolvedIdGit] = field(
        default_factory=dict[UnresolvedIdGit, ResolvedIdGit]
    )
    local_resolvents: dict[UnresolvedIdLocal, ResolvedIdLocal] = field(
        default_factory=dict[UnresolvedIdLocal, ResolvedIdLocal]
    )

    def update(self, unresolved: UnresolvedId, resolved: ResolvedId):
        if unresolved.kind() != resolved.kind():
            raise ValueError("cannot resolve id to different kind")
        match unresolved.kind():
            case SourceKind.Local:
                self.local_resolvents[cast(UnresolvedIdLocal, unresolved)] = cast(ResolvedIdLocal, resolved)
            case SourceKind.Git:
                self.git_resolvents[cast(UnresolvedIdGit, unresolved)] = cast(ResolvedIdGit, resolved)
            case SourceKind.Registry:
                self.registry_resolvents[cast(UnresolvedIdRegistry, unresolved)] = cast(
                    ResolvedIdRegistry, resolved
                )
