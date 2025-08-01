from abc import ABC, abstractmethod
from collections.abc import Generator
from dataclasses import dataclass
from typing import override

from quackpack.solver.types.resolved_id import ResolvedId, ResolvedIdGit, ResolvedIdLocal, ResolvedIdRegistry
from quackpack.solver.types.resolved_package import (
    ResolvedPackage,
    ResolvedPackageGit,
    ResolvedPackageLocal,
    ResolvedPackageRegistry,
)


class PackagesById:
    def __init__(self):
        self.root: dict[ResolvedId, PossiblePackagesFromId] = {}

    def __getitem__(self, key: ResolvedId):
        if key in self.root:
            return self.root[key]
        if isinstance(key, ResolvedIdRegistry):
            self.root[key] = PossibleRegistryPackages(set())
        elif isinstance(key, ResolvedIdGit):
            self.root[key] = PossibleGitPackages(None)
        elif isinstance(key, ResolvedIdLocal):
            self.root[key] = PossibleLocalPackages(None)
        else:
            raise NotImplementedError
        return self.root[key]

    def items(self):
        return self.root.items()

    def keys(self):
        return self.root.keys()

    def values(self):
        return self.root.values()

    def __delitem__(self, key: ResolvedId):
        if key not in self.root:
            raise KeyError
        del self.root[key]

    def __contains__(self, item: ResolvedId) -> bool:
        return item in self.root


class PossiblePackagesFromId(ABC):
    @abstractmethod
    def __iter__(self) -> Generator[ResolvedPackage]: ...

    @abstractmethod
    def add(self, new_package: ResolvedPackage): ...

    @abstractmethod
    def is_empty(self) -> bool: ...

    @abstractmethod
    def remove(self, key: ResolvedPackage): ...


@dataclass
class PossibleRegistryPackages(PossiblePackagesFromId):
    packages: set[ResolvedPackageRegistry]

    @override
    def __iter__(self) -> Generator[ResolvedPackageRegistry]:
        yield from self.packages

    @override
    def add(self, new_package: ResolvedPackage):
        assert isinstance(new_package, ResolvedPackageRegistry)
        self.packages.add(new_package)

    @override
    def is_empty(self):
        return len(self.packages) == 0

    @override
    def remove(self, key: ResolvedPackage):
        if not isinstance(key, ResolvedPackageRegistry):
            raise KeyError
        self.packages.remove(key)


@dataclass
class PossibleGitPackages(PossiblePackagesFromId):
    package: ResolvedPackageGit | None

    @override
    def __iter__(self) -> Generator[ResolvedPackageGit]:
        if self.package is not None:
            yield self.package

    @override
    def add(self, new_package: ResolvedPackage):
        assert isinstance(new_package, ResolvedPackageGit)
        assert self.package is None or new_package == self.package
        self.package = new_package

    @override
    def is_empty(self):
        return self.package is None

    @override
    def remove(self, key: ResolvedPackage):
        if key != self.package:
            raise KeyError
        self.package = None


@dataclass
class PossibleLocalPackages(PossiblePackagesFromId):
    package: ResolvedPackageLocal | None

    @override
    def __iter__(self) -> Generator[ResolvedPackageLocal]:
        if self.package is not None:
            yield self.package

    @override
    def add(self, new_package: ResolvedPackage):
        assert isinstance(new_package, ResolvedPackageLocal)
        assert self.package is None or new_package == self.package
        self.package = new_package

    @override
    def is_empty(self):
        return self.package is None

    @override
    def remove(self, key: ResolvedPackage):
        if key != self.package:
            raise KeyError
        self.package = None
