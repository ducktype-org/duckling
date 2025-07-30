from collections.abc import Iterable
from typing import cast

from quackpack.manifest.dependency import Dependency
from quackpack.manifest.source import GitSource
from quackpack.solver.gathering import GatheredInfo
from quackpack.solver.types.packages_by_id import PackagesById
from quackpack.solver.types.resolved_id import ResolvedIdGit
from quackpack.solver.types.unresolved_id import IdResolvents, UnresolvedId
from quackpack.solver.types.unresolved_package import ResolvedPackage, UnresolvedPackage
from quackpack.util.types.pkgid import GitPackageId


def get_possible_realizations(
    dependency: Dependency, packages_by_id: PackagesById, id_resolvents: IdResolvents
) -> list[ResolvedPackage]:
    if dependency.is_pinned:
        pinned = UnresolvedPackage.create(
            UnresolvedId.from_dependency(dependency), dependency.versions[0]
        ).resolve(id_resolvents)
        if pinned not in packages_by_id[pinned.id]:
            return []
        else:
            return [pinned]
    else:
        id = UnresolvedId.from_dependency(dependency)
        baseline_packages = [
            UnresolvedPackage.create(id, version).resolve(id_resolvents)
            for version in ((None,) if id.is_local() else dependency.versions)
        ]
        return [
            package
            for package in packages_by_id[id.resolve(id_resolvents)]
            # NOTE: Selector to any version should be None instead of [],
            # similarily to system and arch selectors.
            # However the amount of changes to schemas required for that
            # is not worth the effort.
            if dependency.versions == [] or package.is_compatible_with_any(baseline_packages)
        ]


def create_git_fetch_cache(
    data: GatheredInfo, used_packages: Iterable[ResolvedPackage]
) -> dict[GitSource, GitPackageId]:
    result = dict[GitSource, GitPackageId]()
    used_gits = {cast(ResolvedIdGit, pkg.id) for pkg in used_packages if pkg.id.is_git()}
    for unresolved, resolved in data.id_resolvents.git_resolvents.items():
        if resolved not in used_gits:
            continue
        result[
            GitSource(
                git_url=unresolved.repository_url,
                commit=unresolved.commit,
                tag=unresolved.tag,
                branch=unresolved.branch,
            )
        ] = GitPackageId(url=resolved.repository_url, commit=resolved.commit)
    return result
