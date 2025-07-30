from collections import defaultdict
from collections.abc import Iterable
from dataclasses import dataclass
from typing import Any

from pyscipopt import Model  # type: ignore reportUnknownVariableType

from quackpack.solver.solving.package_and_dependency import PackageAndDependencyId
from quackpack.solver.types.flag_type import FeatureId, NoFlag
from quackpack.solver.types.unresolved_package import ResolvedPackage
from quackpack.util.types.pkgid import Identifier

type SCIPModelType = Any
type SCIPVariableType = Any


def _package_var_name(package: ResolvedPackage) -> str:
    return f"{package.id!s}@{package.version!s}"


def _package_with_flag_var_name(package: ResolvedPackage, flag: Identifier) -> str:
    return f"{_package_var_name(package)}@{flag!s}"


def _dependency_flag_realization_var_name(dependency: PackageAndDependencyId, flag: Identifier) -> str:
    return f"{_package_var_name(dependency.parent_package)}->{dependency.dependency_id!s}@_@{flag!s}"


def _dependency_version_realization_var_name(
    dependency: PackageAndDependencyId, realization: ResolvedPackage
):
    return f"{_package_var_name(dependency.parent_package)}->{dependency.dependency_id!s}@{realization!s}@_"


@dataclass
class SolverModelOutput:
    packages: set[ResolvedPackage]
    package_flags: defaultdict[ResolvedPackage, set[Identifier]]
    dep_versions: defaultdict[PackageAndDependencyId, set[ResolvedPackage]]


class SolverModel:
    def __init__(self):
        self.model = Model()  # type: ignore reportUnknownVariableType
        self.model.hideOutput()  # type: ignore reportUnknownMemberType
        self.packages: dict[ResolvedPackage, SCIPVariableType] = {}
        self.package_flags: defaultdict[ResolvedPackage, dict[Identifier, SCIPVariableType]] = defaultdict(
            dict
        )
        self.dep_versions: defaultdict[PackageAndDependencyId, dict[ResolvedPackage, SCIPVariableType]] = (
            defaultdict(dict)
        )
        self.dep_flags: defaultdict[PackageAndDependencyId, dict[Identifier, SCIPVariableType]] = defaultdict(
            dict
        )

    # Adding vars
    # -----------

    def add_package_var(self, package: ResolvedPackage):
        if package in self.packages:
            return
        var_name = _package_var_name(package=package)
        self.packages[package] = self.model.addVar(var_name, vtype="BINARY")  # type: ignore reportUnknownMemberType

    def add_package_with_flag_var(self, package: ResolvedPackage, flag: Identifier):
        if package in self.package_flags and flag in self.package_flags[package]:
            return
        var_name = _package_with_flag_var_name(package, flag)
        self.package_flags[package][flag] = self.model.addVar(var_name, vtype="BINARY")  # type: ignore reportUnknownMemberType

    def add_dependency_version_realization_var(
        self, dependency: PackageAndDependencyId, realization: ResolvedPackage
    ):
        if dependency in self.dep_versions and realization in self.dep_versions[dependency]:
            return
        var_name = _dependency_version_realization_var_name(dependency, realization)
        self.dep_versions[dependency][realization] = self.model.addVar(var_name, vtype="BINARY")  # type: ignore reportUnknownMemberType

    def add_dependency_flag_realization_var(self, dependency: PackageAndDependencyId, flag: Identifier):
        if dependency in self.dep_flags and flag in self.dep_flags[dependency]:
            return
        var_name = _dependency_flag_realization_var_name(dependency, flag)
        self.dep_flags[dependency][flag] = self.model.addVar(var_name, vtype="BINARY")  # type: ignore reportUnknownMemberType

    # Constraints
    # -----------

    def _implies_all_any(
        self, when_all: Iterable[SCIPVariableType], then_any: Iterable[SCIPVariableType]
    ) -> None:
        self.model.addCons(sum(when_all) <= sum(then_any) - 1 + len(when_all))  # type: ignore reportUnknownMemberType

    def _implies_one_all(self, when_one: SCIPVariableType, then_all: Iterable[SCIPVariableType]) -> None:
        self.model.addCons(when_one * len(then_all) <= sum(then_all))  # type: ignore reportUnknownMemberType

    def _implies(self, when: SCIPVariableType, then: SCIPVariableType) -> None:
        self.model.addCons(when <= then)  # type: ignore reportUnknownMemberType

    def require_satisfy_dep_flags(
        self,
        dependency: PackageAndDependencyId,
        parent_flag: Identifier | NoFlag,
        child_flags: set[Identifier],
    ):
        parent_var = self._get_package_var(package=dependency.parent_package, flag=parent_flag)
        self._implies_one_all(
            when_one=parent_var, then_all=[self.dep_flags[dependency][flag] for flag in child_flags]
        )

    def require_satisfy_dep_version(
        self, dependency: PackageAndDependencyId, parent_flag: Identifier | NoFlag
    ):
        parent_var = self._get_package_var(package=dependency.parent_package, flag=parent_flag)
        self._implies_all_any(
            when_all=(parent_var,), then_any=[v for _, v in self.dep_versions[dependency].items()]
        )

    def require_substantiate_dep(self, dependency: PackageAndDependencyId):
        for package, version_realization_var in self.dep_versions[dependency].items():
            package_var = self.packages[package]
            self._implies(when=version_realization_var, then=package_var)

    def require_substantiate_dep_flags(
        self,
        dependency: PackageAndDependencyId,
        possible_flags: dict[ResolvedPackage, set[FeatureId]],
        possible_dependency_realizations: list[ResolvedPackage],
    ):
        for package in possible_dependency_realizations:
            version_realization_var = self.dep_versions[dependency][package]
            for flag, flag_realization_var in self.dep_flags[dependency].items():
                # If the dependency is realized with a given flag, we install it with that flag.
                if flag not in possible_flags[package]:
                    # It is impossible to install package with flag (due to info cleaning, its dependencies failed to fetch).
                    self._implies_all_any(
                        when_all=(version_realization_var, flag_realization_var), then_any=()
                    )
                else:
                    self._implies_all_any(
                        when_all=(version_realization_var, flag_realization_var),
                        then_any=(self.package_flags[package][flag],),
                    )

    def require_package(self, package: ResolvedPackage):
        self.model.addCons(self.packages[package] == 1)  # type: ignore reportUnknownMemberType

    def require_package_with_flag(self, package: ResolvedPackage, flag: Identifier):
        self.model.addCons(self.package_flags[package][flag] == 1)  # type: ignore reportUnknownMemberType

    # Getters
    # -------

    def _get_package_var(self, package: ResolvedPackage, flag: NoFlag | Identifier) -> SCIPVariableType:
        if flag is NoFlag.NoFlag:
            return self.packages[package]
        else:
            return self.package_flags[package][flag]

    # Output
    # ------

    def _is_one(self, var: SCIPVariableType) -> bool:
        return self.model.getVal(var) >= 0.5  # type: ignore reportUnknownVariableType

    def solve(self) -> SolverModelOutput:
        self.model.setObjective(  # type: ignore reportUnknownMemberType
            sum(v for _, v in self.packages.items()), sense="minimize"
        )
        self.model.optimize()  # type: ignore reportUnknownMemberType
        return SolverModelOutput(
            packages={package for package, var in self.packages.items() if self._is_one(var)},
            package_flags=defaultdict(
                set,
                {
                    package: {flag for flag, var in flags.items() if self._is_one(var)}
                    for package, flags in self.package_flags.items()
                },
            ),
            dep_versions=defaultdict(
                set,
                {
                    dep: {pkg for pkg, var in pkgs.items() if self._is_one(var)}
                    for dep, pkgs in self.dep_versions.items()
                },
            ),
        )
