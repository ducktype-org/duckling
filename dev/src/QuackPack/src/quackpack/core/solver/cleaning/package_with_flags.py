from collections.abc import Iterator
from dataclasses import dataclass

from quackpack.core.solver.types.flag_type import FlagType, NoFlag
from quackpack.core.solver.types.resolved_id import ResolvedId
from quackpack.core.solver.types.unresolved_id import IdResolvents, UnresolvedId
from quackpack.core.solver.types.unresolved_package import ResolvedPackage
from quackpack.core.types.manifest.dependency import Dependency
from quackpack.util.types.pkgid import Identifier


@dataclass
class PackageWithFlags:
    package: ResolvedPackage
    flags: tuple[FlagType, FlagType]


@dataclass
class PackageWithFlagsDependency:
    parent_package: PackageWithFlags
    dependency_id: ResolvedId

    def parent_flags(self) -> Iterator[Identifier]:
        return (flag for flag in self.parent_package.flags if flag is not NoFlag.NoFlag)

    @staticmethod
    def from_dependency(
        parent_package: PackageWithFlags,
        dependency: Dependency,
        id_resolvents: IdResolvents,
    ) -> PackageWithFlagsDependency:
        return PackageWithFlagsDependency(
            parent_package,
            UnresolvedId.from_dependency(dependency).resolve(id_resolvents),
        )
