from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import override

from quackpack.core.fetcher.api_types import Package
from quackpack.core.solver.types.resolved_package import (
    ResolvedPackage,
    ResolvedPackageGit,
    ResolvedPackageLocal,
    ResolvedPackageRegistry,
)
from quackpack.core.solver.types.unresolved_id import (
    IdResolvents,
    UnresolvedId,
    UnresolvedIdGit,
    UnresolvedIdLocal,
    UnresolvedIdRegistry,
)
from quackpack.core.types.manifest.source import SourceKind
from quackpack.util.types.version import Version


class UnresolvedPackage(ABC):
    @property
    @abstractmethod
    def id(self) -> UnresolvedId: ...

    @property
    @abstractmethod
    def version(self) -> Version | None: ...

    @abstractmethod
    def resolve(self, id_resolvents: IdResolvents) -> ResolvedPackage: ...

    @staticmethod
    def create(id: UnresolvedId, version: Version | None) -> UnresolvedPackage:
        match id.kind():
            case SourceKind.Registry:
                assert version is not None
                assert isinstance(id, UnresolvedIdRegistry)
                return UnresolvedPackageRegistry(_id=id, _version=version)
            case SourceKind.Git:
                assert version is not None
                assert isinstance(id, UnresolvedIdGit)
                return UnresolvedPackageGit(_id=id, _version=version)
            case SourceKind.Local:
                assert version is None
                assert isinstance(id, UnresolvedIdLocal)
                return UnresolvedPackageLocal(_id=id)


@dataclass(frozen=True)
class UnresolvedPackageRegistry(UnresolvedPackage):
    _id: UnresolvedIdRegistry
    _version: Version

    @property
    @override
    def id(self) -> UnresolvedIdRegistry:
        return self._id

    @property
    @override
    def version(self) -> Version:
        return self._version

    @override
    def resolve(self, id_resolvents: IdResolvents) -> ResolvedPackageRegistry:
        return ResolvedPackageRegistry(self.id.resolve(id_resolvents), self.version)

    def get_package(self) -> Package:
        return Package(id=self.id.package_name, version=str(self.version))

    @override
    def __eq__(self, other: object) -> bool:
        return (
            isinstance(other, UnresolvedPackageRegistry)
            and self.id == other.id
            and self.version == other.version
        )

    @override
    def __hash__(self) -> int:
        return hash((self.id, self.version))


@dataclass(frozen=True)
class UnresolvedPackageGit(UnresolvedPackage):
    _id: UnresolvedIdGit
    _version: Version

    @property
    @override
    def id(self) -> UnresolvedIdGit:
        return self._id

    @property
    @override
    def version(self) -> Version:
        return self._version

    @override
    def resolve(self, id_resolvents: IdResolvents) -> ResolvedPackageGit:
        return ResolvedPackageGit(self.id.resolve(id_resolvents), self.version)

    @override
    def __eq__(self, other: object) -> bool:
        return (
            isinstance(other, UnresolvedPackageGit) and self.id == other.id and self.version == other.version
        )

    @override
    def __hash__(self) -> int:
        return hash((self.id, self.version))


@dataclass(frozen=True)
class UnresolvedPackageLocal(UnresolvedPackage):
    _id: UnresolvedIdLocal

    @property
    @override
    def id(self) -> UnresolvedIdLocal:
        return self._id

    @property
    @override
    def version(self) -> None:
        return None

    @override
    def resolve(self, id_resolvents: IdResolvents) -> ResolvedPackageLocal:
        return ResolvedPackageLocal(self.id.resolve(id_resolvents))

    @override
    def __eq__(self, other: object) -> bool:
        return isinstance(other, UnresolvedPackageLocal) and self.id == other.id

    @override
    def __hash__(self) -> int:
        return hash(self.id)
