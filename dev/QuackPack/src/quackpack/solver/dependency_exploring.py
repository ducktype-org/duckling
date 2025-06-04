from quackpack.config.project import DependencyEntry, GitEntry, LocalEntry, Manifest, VersionList
from quackpack.solver.universal_names import UniversalName
from quackpack.solver.util import NoFlags, SolverPackage, SolverPackageWithFlag, check_conditions
from quackpack.util.pkgid import Identifier
from quackpack.util.version import Version


class DependencyConstructor:
    # Inputs
    # ------
    gathered_configs: dict[SolverPackage, Manifest]
    # Mapping package -> its configuration.

    versions_by_universal_name: dict[UniversalName, list[Version | GitEntry | LocalEntry]]
    # Mapping universal name -> all of its gathered versions.

    possible_flags: dict[SolverPackage, set[NoFlags | str]]
    # All of the packages which may be used in the dependency solution with all of the flags which may be used.

    # Outputs
    # -------
    parents: dict[SolverPackageWithFlag, list[SolverPackageWithFlag]]
    # Mapping package -> list of packages which have it as dependency.

    n_children: dict[SolverPackageWithFlag, dict[UniversalName, dict[str | NoFlags, int]]]
    # Mapping (package, dep_universal_name, dep_flag_name) -> number of options.

    def __init__(
        self,
        gathered_configs: dict[SolverPackage, Manifest],
        versions_by_universal_name: dict[UniversalName, list[Version | GitEntry | LocalEntry]],
        possible_flags: dict[SolverPackage, set[NoFlags | str]],
    ):
        self.gathered_configs = gathered_configs
        self.versions_by_universal_name = versions_by_universal_name
        self.possible_flags = possible_flags

        self.n_children = {}
        self.parents = {}

    def explore_single_dependency(
        self, origin_package: SolverPackageWithFlag, dep_uni_name: UniversalName, dep_entry: DependencyEntry
    ) -> None:
        """Constructs the results for a single dependency entry.

        Args:
            origin_package (SolverPackageWithFlag): Pair (package, flag) for which we are conducting the exploration.
            dep_uni_name (UniversalName): Universal name of the dependency.
            dep_entry (DependencyEntry): Dependency entry describing the dependency.
        """
        if not check_conditions(origin_package.flag, dep_entry.conditions):
            return

        # Necesseary flags of the dependency.
        flags: list[NoFlags | str] = [NoFlags.NO_FLAGS]
        for flag_entry in dep_entry.flags:
            if isinstance(flag_entry, Identifier):
                flags.append(flag_entry.root)
            else:
                for flag, condition in flag_entry.items():
                    # TODO: Is this right, or should it be check_conditions(flag, condition)?
                    if check_conditions(origin_package.flag, condition):
                        flags.append(flag.root)

        self.n_children[origin_package][dep_uni_name] = {}
        for flag in flags:
            self.n_children[origin_package][dep_uni_name][flag] = 0

        # Check all the packages with given universal_name, to find all of the versions which can satisfy the dependency.
        for potential_realisation in self.versions_by_universal_name[dep_uni_name]:
            success = False

            if isinstance(potential_realisation, Version) and isinstance(dep_entry.version, VersionList):
                for version in dep_entry.version:
                    if version.can_be_upgraded_to(potential_realisation):
                        success = True
                        break
            # Universal names for local and git packages are unique so potential_realisation realises dependency.
            elif type(potential_realisation) is type(dep_entry.version):
                success = True

            if success:
                # Update n_children
                for flag in flags:
                    self.n_children[origin_package][dep_uni_name][flag] += 1

                # Update parents
                for flag in flags:
                    dep_solv_pack_wf = SolverPackageWithFlag(
                        package=SolverPackage(name=dep_uni_name, version=potential_realisation), flag=flag
                    )
                    if dep_solv_pack_wf not in self.parents:
                        self.parents[dep_solv_pack_wf] = []
                    self.parents[dep_solv_pack_wf].append(origin_package)

    def explore_single_package(self, package: SolverPackageWithFlag) -> None:
        """Constructs the results for a single package.

        Args:
            package (SolverPackageWithFlag): Pair (package, flag) for which we are conducting the exploration.
        """
        if package not in self.n_children:
            self.n_children[package] = {}

        config = self.gathered_configs[package.package]
        for dep_name, dep_entry in config.dependencies.items():
            self.explore_single_dependency(
                origin_package=package,
                dep_uni_name=UniversalName.create_from_dep_entry(dep_name=dep_name, dep_entry=dep_entry),
                dep_entry=dep_entry,
            )

    def explore_dependencies(self):
        """Constructs the results."""
        for package, flags in self.possible_flags.items():
            for flag in flags:
                self.explore_single_package(SolverPackageWithFlag(package=package, flag=flag))
