from collections.abc import Iterator
from dataclasses import dataclass
from itertools import product

from quackpack.core.solver.cleaning.package_with_flags import PackageWithFlags, PackageWithFlagsDependency
from quackpack.core.solver.gathering import GatheredInfo
from quackpack.core.solver.types.flag_type import FeatureId, FlagType, NoFlag
from quackpack.core.solver.types.packages_by_id import PackagesById
from quackpack.core.solver.types.unresolved_id import IdResolvents
from quackpack.core.solver.types.unresolved_package import ResolvedPackage
from quackpack.core.solver.util import get_possible_realizations
from quackpack.core.types.manifest.dependency import Dependency
from quackpack.core.types.manifest.summary import Summary
from quackpack.util.logger import get_logger

logger = get_logger(__name__)


@dataclass
class CleanerGraph:
    parents: dict[PackageWithFlags, list[PackageWithFlags]]
    """
    List of packages that can depend on a given package (edges of the graph).
    """

    possible_realization_counts: dict[PackageWithFlagsDependency, dict[tuple[FlagType, FlagType], int]]
    """
    For a given parent node (package with two flags) and id of its dependency,
    counts number of dependency realizations grouped by their carried flags (pairs of flags forced by the parent).
    When such a count drops to zero, it means that the dependency cannot be safisfied,
    as for the given flag pair, no realizations have that flag pair.
    """


class _DependencyExplorer:
    def __init__(
        self,
        summaries: dict[ResolvedPackage, Summary],
        packages_by_id: PackagesById,
        possible_flags: dict[ResolvedPackage, set[FeatureId]],
        id_resolvents: IdResolvents,
    ):
        self.summaries = summaries
        self.packages_by_id = packages_by_id
        self.possible_flags = possible_flags
        self.id_resolvents = id_resolvents

    def _explore_single_dependency(
        self, output: CleanerGraph, dependency: PackageWithFlagsDependency, dependency_spec: Dependency
    ) -> None:
        if not dependency_spec.is_enabled(dependency.parent_flags()):
            return

        forced_flags: set[FeatureId] = set(dependency_spec.enabled_features(dependency.parent_flags()))

        for flag1, flag2 in _flag_pairs(forced_flags):
            output.possible_realization_counts.setdefault(dependency, {})[flag1, flag2] = 0

        for realization in get_possible_realizations(
            dependency_spec, self.packages_by_id, self.id_resolvents
        ):
            for flag1, flag2 in _flag_pairs(forced_flags):
                if (
                    flag1 not in self.possible_flags[realization]
                    or flag2 not in self.possible_flags[realization]
                ):
                    continue
                output.possible_realization_counts[dependency][flag1, flag2] += 1
                output.parents.setdefault(PackageWithFlags(realization, (flag1, flag2)), []).append(
                    dependency.parent_package
                )

    def _explore_single_package(self, output: CleanerGraph, package: PackageWithFlags):
        summary = self.summaries[package.package]
        for _, dependency_spec in summary.deps.items():
            self._explore_single_dependency(
                output,
                PackageWithFlagsDependency.from_dependency(package, dependency_spec, self.id_resolvents),
                dependency_spec,
            )

    def explore_dependencies(self, output: CleanerGraph):
        for package, flags in self.possible_flags.items():
            for flag1, flag2 in _flag_pairs(flags):
                self._explore_single_package(output, PackageWithFlags(package, (flag1, flag2)))
        return output


def _flag_pairs(flags: set[FeatureId]) -> Iterator[tuple[FlagType, FlagType]]:
    """
    Returns iterator of those flag pairs, such that elements of each pair are sorted in order.
    `NoFlags` is added to the flag set and it is considered the greatest element.
    """
    return (
        (f1, f2)
        for (f1, f2) in product(flags | {NoFlag.NoFlag}, repeat=2)
        if f2 == NoFlag.NoFlag or (f1 != NoFlag.NoFlag and str(f1) <= str(f2))
    )


def create_cleaner_graph(data: GatheredInfo) -> CleanerGraph:
    output = CleanerGraph(parents={}, possible_realization_counts={})
    explorer = _DependencyExplorer(
        data.summaries, data.packages_by_id, data.possible_features, data.id_resolvents
    )
    explorer.explore_dependencies(output)
    return output
