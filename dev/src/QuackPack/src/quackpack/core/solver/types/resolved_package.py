from abc import ABC, abstractmethod
from collections.abc import Iterable
from dataclasses import dataclass
from typing import override

from quackpack.core.solver.types.resolved_id import (
    ResolvedId,
    ResolvedIdGit,
    ResolvedIdLocal,
    ResolvedIdRegistry,
)
from quackpack.util.types.pkgid import (
    GitPackageId,
    LocalPackageId,
    PackageId,
    RegistryPackageId,
)
from quackpack.util.types.version import Version


class ResolvedPackage(ABC):
    @property
    @abstractmethod
    def id(self) -> ResolvedId: ...

    @property
    @abstractmethod
    def version(self) -> Version | None: ...

    @abstractmethod
    def is_compatible_with(self, other: ResolvedPackage) -> bool: ...

    def is_compatible_with_any(self, others: Iterable[ResolvedPackage]) -> bool:
        return any(self.is_compatible_with(other) for other in others)

    @abstractmethod
    def to_package_id(self) -> PackageId: ...


@dataclass(frozen=True)
class ResolvedPackageRegistry(ResolvedPackage):
    _id: ResolvedIdRegistry
    _version: Version

    @property
    @override
    def id(self) -> ResolvedIdRegistry:
        return self._id

    @property
    @override
    def version(self) -> Version:
        return self._version

    @override
    def is_compatible_with(self, other: ResolvedPackage) -> bool:
        return (
            isinstance(other, ResolvedPackageRegistry)
            and self.id == other.id
            and other.version.can_be_upgraded_to(self.version)
        )

    @override
    def to_package_id(self) -> PackageId:
        return RegistryPackageId(
            url=self.id.registry_url, id=self.id.package_name, version=self.version
        ).upcast()

    @override
    def __eq__(self, other: object) -> bool:
        return (
            isinstance(other, ResolvedPackageRegistry)
            and self.id == other.id
            and self.version == other.version
        )

    @override
    def __hash__(self) -> int:
        return hash((self.id, self.version))


@dataclass(frozen=True)
class ResolvedPackageGit(ResolvedPackage):
    _id: ResolvedIdGit
    _version: Version

    @property
    @override
    def id(self) -> ResolvedIdGit:
        return self._id

    @property
    @override
    def version(self) -> Version:
        return self._version

    @override
    def is_compatible_with(self, other: ResolvedPackage) -> bool:
        return self == other

    @override
    def to_package_id(self) -> PackageId:
        return GitPackageId(url=self.id.repository_url, commit=self.id.commit).upcast()

    @override
    def __eq__(self, other: object) -> bool:
        return (
            isinstance(other, ResolvedPackageGit)
            and self.id == other.id
            and self.version == other.version
        )

    @override
    def __hash__(self) -> int:
        return hash((self.id, self.version))


@dataclass(frozen=True)
class ResolvedPackageLocal(ResolvedPackage):
    _id: ResolvedIdLocal

    @property
    @override
    def id(self) -> ResolvedIdLocal:
        return self._id

    @property
    @override
    def version(self) -> None:
        return None

    @override
    def is_compatible_with(self, other: ResolvedPackage) -> bool:
        return self == other

    @override
    def to_package_id(self) -> PackageId:
        return LocalPackageId(path=self.id.local_path).upcast()

    @override
    def __eq__(self, other: object) -> bool:
        return isinstance(other, ResolvedPackageLocal) and self.id == other.id

    @override
    def __hash__(self) -> int:
        return hash(self.id)
