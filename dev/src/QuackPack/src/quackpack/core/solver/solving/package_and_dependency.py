from dataclasses import dataclass
from typing import override

from quackpack.core.solver.types.resolved_id import ResolvedId
from quackpack.core.solver.types.unresolved_id import IdResolvents, UnresolvedId
from quackpack.core.solver.types.unresolved_package import ResolvedPackage
from quackpack.core.types.manifest.dependency import Dependency


@dataclass
class PackageAndDependencyId:
    parent_package: ResolvedPackage
    dependency_id: ResolvedId

    @staticmethod
    def from_dependency(
        parent_package: ResolvedPackage,
        dependency: Dependency,
        id_resolvents: IdResolvents,
    ) -> PackageAndDependencyId:
        return PackageAndDependencyId(
            parent_package,
            UnresolvedId.from_dependency(dependency).resolve(id_resolvents),
        )

    @override
    def __eq__(self, other: object) -> bool:
        return (
            isinstance(other, PackageAndDependencyId)
            and self.parent_package == other.parent_package
            and self.dependency_id == other.dependency_id
        )

    @override
    def __hash__(self):
        return hash((self.parent_package, self.dependency_id))
