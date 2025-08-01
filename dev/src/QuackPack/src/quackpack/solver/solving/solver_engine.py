from quackpack.manifest.dependency import Dependency
from quackpack.solver.gathering import GatheredInfo
from quackpack.solver.solving.package_and_dependency import PackageAndDependencyId
from quackpack.solver.solving.solver_model import SolverModel, SolverModelOutput
from quackpack.solver.types.flag_type import FeatureId, NoFlag
from quackpack.solver.types.unresolved_package import ResolvedPackage
from quackpack.solver.util import get_possible_realizations
from quackpack.util.logger import get_logger

logger = get_logger(__name__)


class _SolverEngine:
    def __init__(self, data: GatheredInfo):
        self.data = data
        self.model = SolverModel()
        logger.debug(
            f"New SolverEngine {{ gathered_manifests: {data.summaries}, packages_by_id: {data.packages_by_id}, possible_flags: {data.possible_features} }}"
        )

    def _create_dependency_version_realization_conditions(
        self,
        dependency: PackageAndDependencyId,
        dependency_description: Dependency,
        possible_realizations: list[ResolvedPackage],
    ) -> None:
        for realization in possible_realizations:
            self.model.add_dependency_version_realization_var(dependency, realization)

        is_forced_default = dependency_description.is_enabled([])
        if is_forced_default:
            self.model.require_satisfy_dep_version(dependency, NoFlag.NoFlag)
        for dependency_forcing_flag in self.data.possible_features[dependency.parent_package]:
            if dependency_description.is_enabled(dependency_forcing_flag) and not is_forced_default:
                self.model.require_satisfy_dep_version(dependency, dependency_forcing_flag)

    def _create_dependency_flag_realization_conditions(
        self, dependency: PackageAndDependencyId, dependency_description: Dependency
    ) -> None:
        origin_flags = self.data.possible_features[dependency.parent_package]

        enabled_always = list(dependency_description.enabled_features([]))
        for origin_flag in origin_flags | {NoFlag.NoFlag}:
            if origin_flag is NoFlag.NoFlag:
                forced = set(enabled_always)
            else:
                forced = set(dependency_description.enabled_features(origin_flag)).difference(enabled_always)

            if not forced:
                continue

            for flag in forced:
                self.model.add_dependency_flag_realization_var(dependency, flag)

            self.model.require_satisfy_dep_flags(dependency, origin_flag, forced)

    def _construct_single_dependency(
        self, parent_package: ResolvedPackage, dependency_description: Dependency
    ) -> None:
        possible_realizations = get_possible_realizations(
            dependency_description, self.data.packages_by_id, self.data.id_resolvents
        )
        dependency = PackageAndDependencyId.from_dependency(
            parent_package, dependency_description, self.data.id_resolvents
        )
        self._create_dependency_version_realization_conditions(
            dependency, dependency_description, possible_realizations
        )
        self._create_dependency_flag_realization_conditions(dependency, dependency_description)

        self.model.require_substantiate_dep(dependency)
        self.model.require_substantiate_dep_flags(
            dependency, self.data.possible_features, possible_realizations
        )

    def _create_package_variables(self) -> None:
        for package in self.data.summaries:
            self.model.add_package_var(package)
            for flag in self.data.possible_features[package]:
                self.model.add_package_with_flag_var(package, flag)

    def run(self, root_projects: list[tuple[ResolvedPackage, set[FeatureId]]]) -> SolverModelOutput:
        self._create_package_variables()

        for package, summary in self.data.summaries.items():
            possible_flags = self.data.possible_features[package]
            for dependency in summary.deps.values():
                if dependency.is_enabled(possible_flags):
                    self._construct_single_dependency(package, dependency)

        for root_project, enabled_flags in root_projects:
            self.model.require_package(root_project)
            for flag in enabled_flags:
                self.model.require_package_with_flag(root_project, flag)

        return self.model.solve()


def run_engine(
    data: GatheredInfo, root_projects: list[tuple[ResolvedPackage, set[FeatureId]]]
) -> SolverModelOutput:
    return _SolverEngine(data).run(root_projects)
