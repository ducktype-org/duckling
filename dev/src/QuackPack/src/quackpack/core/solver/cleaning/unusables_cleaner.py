from collections import deque

from quackpack.core.solver.cleaning.cleaner_graph_constructor import (
    CleanerGraph,
    create_cleaner_graph,
)
from quackpack.core.solver.cleaning.package_with_flags import (
    PackageWithFlags,
    PackageWithFlagsDependency,
)
from quackpack.core.solver.gathering import GatheredInfo
from quackpack.core.solver.types.flag_type import NoFlag
from quackpack.core.solver.types.unresolved_package import ResolvedPackage
from quackpack.util.logger import get_logger

logger = get_logger(__name__)


class _UnusablesCleaner:
    def __init__(self, cleaner_graph: CleanerGraph):
        self.parents = cleaner_graph.parents
        self.possible_realization_counts = cleaner_graph.possible_realization_counts
        logger.debug(
            f"New UnusablesCleaner {{parents: {cleaner_graph.parents!s}, possible_realization_counts: {cleaner_graph.possible_realization_counts!s}}}"
        )

    def _find_unusable_packages_with_flags(self) -> set[PackageWithFlags]:
        unusables: set[PackageWithFlags] = set()
        unusables_queue: deque[PackageWithFlags] = deque()

        for dependency, counts in self.possible_realization_counts.items():
            if 0 in counts.values():
                unusables_queue.append(dependency.parent_package)

        while unusables_queue:
            package_with_flags = unusables_queue.pop()
            if package_with_flags in unusables:
                continue
            logger.debug(
                f"Package {package_with_flags.package} with two flags {package_with_flags.flags} was found unusable."
            )
            unusables.add(package_with_flags)

            for parent in self.parents[package_with_flags]:
                dependency = PackageWithFlagsDependency(
                    parent, package_with_flags.package.id
                )
                self.possible_realization_counts[dependency][
                    package_with_flags.flags
                ] -= 1
                if (
                    self.possible_realization_counts[dependency][
                        package_with_flags.flags
                    ]
                    == 0
                ):
                    unusables_queue.append(parent)
        return unusables

    def clean_data_structures(self, data: GatheredInfo) -> None:
        unusable_packages_with_flags = self._find_unusable_packages_with_flags()
        unusable_packages = set[ResolvedPackage]()
        for unusable in unusable_packages_with_flags:
            if unusable.flags[0] == unusable.flags[1]:
                unusable_flag = unusable.flags[0]
                if unusable_flag == NoFlag.NoFlag:
                    unusable_packages.add(unusable.package)
                else:
                    logger.debug(
                        f"Package {unusable.package!s} has an unusable flag {unusable_flag!s}"
                    )
                    data.possible_features[unusable.package].remove(unusable_flag)

        for package in unusable_packages:
            logger.debug(f"Package {package!s} is unusable")
            del data.possible_features[package]
            del data.summaries[package]
            data.packages_by_id[package.id].remove(package)

        for id, realizations in data.packages_by_id.items():
            if realizations.is_empty():
                logger.debug(f"There are no usable packages for id {id!s}")
                del data.packages_by_id[id]


def clean_gathered_info(input: GatheredInfo) -> GatheredInfo:
    """
    Cleans the gathered info out of unusable packages (and all packages
    trasitively deemed as unusable).
    Note that the input is consumed and should be discarded after calling this function.
    """
    _UnusablesCleaner(create_cleaner_graph(input)).clean_data_structures(input)
    return input
