from collections import deque

from quackpack.config.project import GitEntry, LocalEntry
from quackpack.solver.universal_names import UniversalName
from quackpack.solver.util import NoFlags, SolverPackage, SolverPackageWithFlag
from quackpack.util.version import Version


class NoResolutionError(Exception):
    def __init__(self):
        super().__init__("The available packages are insufficient for dependency resolution")


# We implement the algorithm described in the bachelor degree paper.
class InfoCleaner:
    # Inputs:
    # -------
    possible_flags: dict[SolverPackage, set[NoFlags | str]]
    # All of the packages which may be used in the dependency solution with all of the flags which may be used.

    versions_by_universal_name: dict[UniversalName, list[Version | GitEntry | LocalEntry]]
    # Mapping universal name -> all of its gathered versions.

    parents: dict[SolverPackageWithFlag, list[SolverPackageWithFlag]]
    # Mapping package -> list of packages which have it as dependency.

    n_children: dict[SolverPackageWithFlag, dict[UniversalName, dict[str | NoFlags, int]]]
    # Mapping (package, dep_universal_name, dep_flag_name) -> number of options

    # Byproducts:
    # --------
    fails: set[SolverPackageWithFlag]
    # All of the unusable pairs (package, flag).

    def __init__(
        self,
        possible_flags: dict[SolverPackage, set[NoFlags | str]],
        fails: set[UniversalName],
        flag_fails: set[SolverPackageWithFlag],
        parents: dict[SolverPackageWithFlag, list[SolverPackageWithFlag]],
        n_children: dict[SolverPackageWithFlag, dict[UniversalName, dict[str | NoFlags, int]]],
        versions_by_universal_name: dict[UniversalName, list[Version | GitEntry | LocalEntry]],
    ):
        self.possible_flags = possible_flags
        self.fails = set()
        self.parents = parents
        self.n_children = n_children
        self.versions_by_universal_name = versions_by_universal_name

        for package, flags in self.possible_flags.items():
            for flag in flags:
                if package.name in fails:
                    self.fails.add(SolverPackageWithFlag(package=package, flag=flag))
        self.fails.update(flag_fails)

    def clean(self) -> None:
        """
        Cleans possible_flags and versions_by_universal_name.
        """
        unusable_packages: set[SolverPackageWithFlag] = set()
        unusables_queue: deque[SolverPackageWithFlag] = deque(list(self.fails))

        while len(unusables_queue) > 0:
            p = unusables_queue.popleft()
            unusable_packages.add(p)
            for parent in self.parents[p]:
                self.n_children[parent][p.package.name][p.flag] -= 1
                if self.n_children[parent][p.package.name][p.flag] == 0 and parent not in unusable_packages:
                    unusables_queue.append(parent)

        # Cleaning possible_flags values.
        for package in unusable_packages:
            self.possible_flags[package.package].remove(package.flag)

        # Cleaning versions_by_universal_name values.
        to_del_packages: list[SolverPackage] = []
        for package, flags in self.possible_flags.items():
            if len(flags) == 0:
                self.versions_by_universal_name[package.name].remove(package.version)
                to_del_packages.append(package)

        # We remove entries with no flags (even NO_FLAGS) from possible_flags.
        for package in to_del_packages:
            del self.possible_flags[package]

        # We remove entries with no versions from versions_by_universal_name.
        to_del_names: list[UniversalName] = []
        for name, versions in self.versions_by_universal_name.items():
            if len(versions) == 0:
                to_del_names.append(name)
        for name in to_del_names:
            del self.versions_by_universal_name[name]
