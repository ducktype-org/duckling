from quackpack.config.project import GitEntry, LocalEntry, Manifest
from quackpack.solver.solver_engine import PackageAndDependency, SCIPModelType, SCIPVariableType
from quackpack.solver.universal_names import NO_NAME, UniversalName
from quackpack.solver.util import SolverPackage
from quackpack.util.version import Version


class BuildRulesConstructor:
    # Inputs:
    # -------
    gathered_configs: dict[SolverPackage, Manifest]
    # Mapping package -> its configuration.

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

    # Outputs:
    # --------
    all_flags: dict[SolverPackage, list[str]]
    # Mapping package -> flags with which it should be installed.
    # Empty list means the package can be installed without any flags.

    instructions: dict[SolverPackage, dict[SolverPackage, list[str]]]
    # Mapping package -> (package realising dependency -> list of flags with which it has to be installed).

    def __init__(
        self,
        gathered_configs: dict[SolverPackage, Manifest],
        variables_dep_version: dict[
            PackageAndDependency, dict[Version | LocalEntry | GitEntry, SCIPVariableType]
        ],
        variables_dep_flag: dict[PackageAndDependency, dict[str, SCIPVariableType]],
        variables_bare: dict[SolverPackage, SCIPVariableType],
        variables_flag: dict[SolverPackage, dict[str, SCIPVariableType]],
        model: SCIPModelType,
    ):
        self.gathered_configs = gathered_configs
        self.variables_dep_version = variables_dep_version
        self.variables_dep_flag = variables_dep_flag
        self.variables_bare = variables_bare
        self.variables_flag = variables_flag
        self.model = model

        self.instructions = {}
        self.all_flags = {}

    def single_package(self, package: SolverPackage) -> None:
        """Construct the results for a single package.

        Args:
            package (SolverPackage): Currently served package.
        """
        package_flags: list[str] = [
            flag
            for flag in self.variables_flag[package]
            if self.model.getSolVal(None, self.variables_flag[package][flag]) >= 0.9
        ]
        self.all_flags[package] = package_flags

        self.instructions[package] = {}
        for dep_name, dep_entry in self.gathered_configs[package].dependencies.items():
            dep_uni_name = UniversalName.create_from_dep_entry(dep_name=dep_name, dep_entry=dep_entry)
            pack_and_dep = PackageAndDependency(parent=package, dep_name=dep_uni_name)
            if dep_uni_name.name == NO_NAME:
                dep_uni_name.name = dep_name
            realisation = next(
                (
                    version
                    for version in self.variables_dep_version[pack_and_dep]
                    if self.model.getSolVal(None, self.variables_dep_version[pack_and_dep][version]) >= 0.9
                ),
                None,
            )
            if realisation is None:
                continue
            dep_package = SolverPackage(name=dep_uni_name, version=realisation)
            flags = [
                flag
                for flag in self.variables_dep_flag[pack_and_dep]
                if self.model.getSolVal(None, self.variables_dep_flag[pack_and_dep][flag]) >= 0.9
            ]
            self.instructions[package][dep_package] = flags

    def construct_instructions(self):
        """Construct the results."""
        for package in self.variables_bare:
            if self.model.getSolVal(None, self.variables_bare[package]) >= 0.9:
                self.single_package(package)
