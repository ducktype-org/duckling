from dataclasses import dataclass
from typing import Any, override

from pyscipopt import Model  # type: ignore reportUnknownVariableType

from quackpack.config.project import DependencyEntry, GitEntry, LocalEntry, Manifest, VersionList
from quackpack.solver.universal_names import UniversalName
from quackpack.solver.util import NoFlags, SolverPackage, check_conditions
from quackpack.util.pkgid import Identifier
from quackpack.util.version import Version


@dataclass
class PackageAndDependency:
    parent: SolverPackage
    dep_name: UniversalName

    @override
    def __hash__(self) -> int:
        return hash((self.parent, self.dep_name))


type SCIPModelType = Any
type SCIPVariableType = Any


class SolverEngine:
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
    variables_bare: dict[SolverPackage, SCIPVariableType]
    # Mapping package -> its general variable.

    variables_flag: dict[SolverPackage, dict[str, SCIPVariableType]]
    # Mapping (package, flag (not NO_FLAGs)) -> pair's variable.

    variables_dep_version: dict[PackageAndDependency, dict[Version | LocalEntry | GitEntry, SCIPVariableType]]
    # Mapping (package and dependency, version) -> version realisation of the dependency variable.

    variables_dep_flag: dict[PackageAndDependency, dict[str, SCIPVariableType]]
    # Mapping (package and dependency, flag (not NO_FLAGS)) -> flag realisation of the dependency variable.

    model: SCIPModelType
    # SCIP model for our ILP solving.

    def __init__(
        self,
        gathered_configs: dict[SolverPackage, Manifest],
        versions_by_universal_name: dict[UniversalName, list[Version | GitEntry | LocalEntry]],
        possible_flags: dict[SolverPackage, set[NoFlags | str]],
    ):
        self.gathered_configs = gathered_configs
        self.versions_by_universal_name = versions_by_universal_name
        self.possible_flags = possible_flags
        self.variables_bare = {}
        self.variables_flag = {}
        self.variables_dep_version = {}
        self.variables_dep_flag = {}
        self.model = Model()

    # Helpers for single dep.
    # -----------------------

    def create_possible_versions(
        self, dep_name: UniversalName, dep_entry: DependencyEntry
    ) -> list[Version | GitEntry | LocalEntry]:
        """Find the possible versions of the dependency-realising package.

        Args:
            dep_name (UniversalName): Universal name of the dependency.
            dep_entry (DependencyEntry): Fragment of the deserialised parent_pack config file describing the dependency.

        Returns:
            list[Version | GitEntry | LocalEntry]: List of the possible versions.
        """
        return (
            [
                dep_version
                for dep_version in self.versions_by_universal_name[dep_name]
                if isinstance(dep_version, Version)
                and any(
                    suggested_version.can_be_upgraded_to(dep_version)
                    for suggested_version in dep_entry.version
                )
            ]
            if isinstance(dep_entry.version, VersionList)
            else [dep_entry.version]
        )

    def create_dep_variables_flag_and_add_flags_conditions(
        self, parent_pack: SolverPackage, dep_name: UniversalName, dep_entry: DependencyEntry
    ) -> dict[str, SCIPVariableType]:
        """Creates the  general flag dependency realisation variables and writes their conditions.

        Args:
            parent_pack (SolverPackage): Package which has the dependency.
            dep_name (UniversalName): Universal name of the dependency.
            dep_entry (DependencyEntry): Fragment of the deserialised parent_pack config file describing the dependency.

        Returns:
            dict[str, SCIPVariableType]: Mapping (for a fixed dependency) flag -> variable.
        """
        dep_variables_flag: dict[str, SCIPVariableType] = {}
        origin_flags = list(self.possible_flags[parent_pack])

        for origin_flag in origin_flags:
            # For each possible flag of the parent package, find what dependency flags it additionally implies.
            forced_flags: set[str] = set()
            for dep_flags_entry in dep_entry.flags:
                if isinstance(dep_flags_entry, Identifier):
                    if origin_flag is NoFlags.NO_FLAGS:
                        forced_flags.add(dep_flags_entry.root)
                else:
                    forced_flags.update(
                        flag.root
                        for flag, cond in dep_flags_entry.items()
                        if check_conditions(origin_flag=origin_flag, conditions=cond)
                    )

            # Write a condition: parent_pack@origin_flag => dependency realisation with all of the forced flags.
            # We start by creating the general variables: parent_pack->dep_name@_@flag.
            if len(forced_flags) == 0:
                continue
            for flag in forced_flags:
                if flag in dep_variables_flag:
                    continue
                dep_variables_flag[flag] = self.model.addVar(
                    f"{parent_pack.name!s}@{parent_pack.version!s}->{dep_name}@_@{flag}", vtype="BINARY"
                )
            # We write the conditions parent_pack with origin flag implies dependency realisation with all of the forced flags.
            # If the origin flag is NO_FLAGS we use the standard parent_pack variable, otherwise we use parent_pack variable with concrete flag.
            origin_variable = (
                self.variables_bare[parent_pack]
                if origin_flag is NoFlags.NO_FLAGS
                else self.variables_flag[parent_pack][origin_flag]
            )
            self.model.addCons(
                sum(dep_variables_flag[x] for x in forced_flags) - len(forced_flags) * origin_variable >= 0
            )
        self.variables_dep_flag[PackageAndDependency(parent=parent_pack, dep_name=dep_name)] = (
            dep_variables_flag
        )
        return dep_variables_flag

    def create_variables_dep_version_and_add_dep_versions_conditions(
        self, parent_pack: SolverPackage, dep_name: UniversalName
    ) -> dict[Version | LocalEntry | GitEntry, SCIPVariableType]:
        """Creates the general dependency version variables and writes their conditions.

        Args:
            parent_pack (SolverPackage): Package which has the dependency.
            dep_name (UniversalName): Universal name of the dependency.

        Returns:
            dict[Version | LocalEntry | GitEntry, SCIPVariableType]: Mapping (for a fixed dependency) version -> variable.
        """
        # Create the general variables: parent_pack->dep_name@version@_.
        dep_variables_version: dict[Version | LocalEntry | GitEntry, SCIPVariableType] = {}
        for potential_dep_version in self.versions_by_universal_name[dep_name]:
            dep_variables_version[potential_dep_version] = self.model.addVar(
                f"{parent_pack.name!s}@{parent_pack.version!s}->{dep_name}@{potential_dep_version!s}@_",
                vtype="BINARY",
            )
        # Add the condition that parent_pack implies the dependency in some version.
        self.model.addCons(
            sum(dep_variables_version[x] for x in dep_variables_version) - self.variables_bare[parent_pack]
            >= 0
        )
        self.variables_dep_version[PackageAndDependency(parent=parent_pack, dep_name=dep_name)] = (
            dep_variables_version
        )
        return dep_variables_version

    def single_dep(
        self, parent_pack: SolverPackage, dep_name: UniversalName, dep_entry: DependencyEntry
    ) -> None:
        """Adds all of the conditions implied by a single dependency.

        Args:
            parent_pack (SolverPackage): Package which has the dependency.
            dep_name (UniversalName): Universal name of the dependency.
            dep_entry (DependencyEntry): Fragment of the deserialised parent_pack config file describing the dependency.
        """
        # Find all of the possible versions, realising the dependency.
        possible_versions = self.create_possible_versions(dep_name=dep_name, dep_entry=dep_entry)

        # General dependency flags conditions.
        dep_variables_flag = self.create_dep_variables_flag_and_add_flags_conditions(
            parent_pack=parent_pack, dep_name=dep_name, dep_entry=dep_entry
        )

        # General dependency versions conditions.
        dep_variables_version = self.create_variables_dep_version_and_add_dep_versions_conditions(
            parent_pack=parent_pack, dep_name=dep_name
        )

        # Version realisation conditions.
        # If the dependency was chosen to be realised with a given version, then the package in that version has to be installed.
        for dep_version, dep_version_variable in dep_variables_version.items():
            dep_variable = self.variables_bare[SolverPackage(name=dep_name, version=dep_version)]
            self.model.addCons(dep_variable - dep_version_variable >= 0)

        # Flag and version realisation conditions.
        # If the dependency was chosen to have a given flag and be realised with a given version,
        # then package in that version has to be installed with that flag.
        for dep_version in possible_versions:
            package = SolverPackage(name=dep_name, version=dep_version)
            for flag, variable in dep_variables_flag.items():
                # If the dependency is realised with a given flag, we install it with that flag.
                if flag not in self.possible_flags[package]:
                    # It is impossible to install package with flag (due to info cleaning, its dependencies failed to fetch).
                    self.model.addCons(variable + dep_variables_version[dep_version] <= 1)
                else:
                    self.model.addCons(
                        self.variables_flag[package][flag] - variable - dep_variables_version[dep_version]
                        >= -1
                    )

    def create_package_variables(self) -> None:
        """
        Creates the variables describing whether a package or a package with a given flag should be installed.
        """
        for uni_name, versions in self.versions_by_universal_name.items():
            for version in versions:
                package = SolverPackage(name=uni_name, version=version)
                self.variables_bare[package] = self.model.addVar(f"{uni_name!s}@{version!s}", vtype="BINARY")
                self.variables_flag[package] = {}
                for flag in self.possible_flags[package]:
                    if flag is NoFlags.NO_FLAGS:
                        continue
                    self.variables_flag[package][flag] = self.model.addVar(
                        f"{uni_name!s}@{version!s}@{flag}", vtype="BINARY"
                    )

    def run_engine(self, main_projects: list[tuple[SolverPackage, list[NoFlags | str]]]) -> None:
        """Main entry-point.

        Args:
            main_projects (list[tuple[SolverPackage, list[NoFlags  |  str]]]): List of the main projects and their flags.
        """
        self.create_package_variables()

        for package in self.possible_flags:
            for dep_name, dep_entry in self.gathered_configs[package].dependencies.items():
                dep_uni_name = UniversalName.create_from_dep_entry(dep_name=dep_name, dep_entry=dep_entry)
                self.single_dep(package, dep_name=dep_uni_name, dep_entry=dep_entry)

        # Add main projects presence constraints.
        for main_project, flags in main_projects:
            self.model.addCons(self.variables_bare[main_project] == 1)
            for flag in flags:
                if flag is not NoFlags.NO_FLAGS:
                    self.model.addCons(self.variables_flag[main_project][flag] == 1)

        # Add the optimisation goal.
        package_vars = (self.variables_bare[x] for x in self.variables_bare)
        package_vars_flags: list[SCIPVariableType] = []
        for package in self.variables_flag:
            for flag in self.variables_flag[package]:
                package_vars_flags.append(self.variables_flag[package][flag])
        self.model.setObjective(sum(package_vars) * 1000 + sum(package_vars_flags), sense="minimize")

        # Optimise.
        self.model.optimize()

        # TODO: Explore SCIP API to possibly limit the execution time (we are happy with not necessairly optimal solutions).
