from abc import ABC, abstractmethod
from dataclasses import dataclass
from pathlib import Path
from typing import override

from quackpack.manifest.summary import Summary
from quackpack.solver.types.flag_type import FeatureId
from quackpack.solver.types.resolved_package import ResolvedPackage
from quackpack.solver.types.unresolved_id import UnresolvedId
from quackpack.solver.types.unresolved_package import UnresolvedPackage
from quackpack.util.types.version import Version


@dataclass(frozen=True, eq=True)
class FetchRequest(ABC):
    flags: frozenset[FeatureId]
    local_root: Path | None

    @abstractmethod
    def target_str(self) -> str: ...


@dataclass(frozen=True, eq=True)
class UnpinnedFetchRequest(FetchRequest):
    id: UnresolvedId
    versions: tuple[Version, ...]

    @override
    def target_str(self) -> str:
        return str(self.id)


@dataclass(frozen=True, eq=True)
class PinnedFetchRequest(FetchRequest):
    pkg: UnresolvedPackage

    @override
    def target_str(self) -> str:
        return str(self.pkg)


@dataclass
class FetchFailure:
    source_request: FetchRequest
    reason: str


@dataclass
class FetchResult:
    packages: dict[ResolvedPackage, Summary]
    request: UnresolvedId | UnresolvedPackage
