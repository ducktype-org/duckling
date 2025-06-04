from pathlib import Path

from quackpack.config.project import LocalEntry, Manifest
from quackpack.fetcher.fetcher import Fetcher
from quackpack.project import Project
from quackpack.solver.build_rules_constructor import BuildRulesConstructor
from quackpack.solver.dependency_exploring import DependencyConstructor
from quackpack.solver.info_cleaning import InfoCleaner, NoResolutionError
from quackpack.solver.info_gathering import InfoGatherer
from quackpack.solver.solver_engine import SolverEngine
from quackpack.solver.universal_names import UniversalName
from quackpack.solver.util import NoFlags, SolverPackage
from quackpack.util.errors import QuackPackError
from quackpack.util.logger import get_logger

logger = get_logger(__name__)


class Solver:
    # Inputs
    # ------

    main_projects_info: list[tuple[Path, list[str]]]
    # List of the projects for which we are solving dependencies, with their flags.

    # Byproducts
    # ----------
    main_projects: list[tuple[SolverPackage, Manifest, LocalEntry, list[NoFlags | str]]]
    # Main_projects_info, translated into solver specific types and with gathered configs.

    # Runners
    # -------
    info_gatherer: InfoGatherer
    dependency_constructor: DependencyConstructor
    info_cleaner: InfoCleaner
    solver_engine: SolverEngine
    build_rules_constructor: BuildRulesConstructor

    def __init__(self, main_projects_info: list[tuple[Path, list[str]]]):
        self.main_projects_info = main_projects_info
        main_projects: list[tuple[SolverPackage, Manifest, LocalEntry, list[NoFlags | str]]] = []
        for path, flags in self.main_projects_info:
            try:
                project = Project(manifest_path=path)
                config = project.manifest_with_acquiring_lock()
            except FileNotFoundError as e:
                raise QuackPackError(reason=e) from None
            name = UniversalName.local_uni_name(path)
            loc_entry = LocalEntry(path=path)
            package = SolverPackage(name=name, version=loc_entry)
            new_flags: list[NoFlags | str] = [NoFlags.NO_FLAGS]
            for flag in flags:
                new_flags.append(flag)
            main_projects.append((package, config, loc_entry, new_flags))
        self.main_projects = main_projects

    async def gather(self, fetcher: Fetcher, allow_unusable: bool) -> None:
        """Conduct the info gathering.

        Args:
            allow_unusable (bool): Whether we allow server fetches to fail.

        Raises:
            QuackPackError: Fetching failed.
        """
        with fetcher as fetcher:
            self.info_gatherer = InfoGatherer(fetcher=fetcher, main_projects=self.main_projects)
            await self.info_gatherer.gather()

            _gathering_failed = any(name.is_server() or allow_unusable for name in self.info_gatherer.fails)
            # TODO: CRITICAL global pres
            # if gathering_failed:
            # TODO: Logging + custom error
            # raise QuackPackError(reason="Some fetches failed")

    def finish_solving(
        self,
    ) -> tuple[dict[SolverPackage, list[str]], dict[SolverPackage, dict[SolverPackage, list[str]]]]:
        """Run dependency_exploring, info_cleaning, solver_engine and build_rules_constructor.

        Raises:
            QuackPackError: No solution was found or something else went wrong.

        Returns:
            tuple[dict[SolverPackage, list[str]], dict[SolverPackage, dict[SolverPackage, list[str]]]]: The final results of build_rules_constructor.
        """
        self.dependency_constructor = DependencyConstructor(
            gathered_configs=self.info_gatherer.gathered_configs,
            versions_by_universal_name=self.info_gatherer.versions_by_universal_name,
            possible_flags=self.info_gatherer.possible_flags,
        )
        self.dependency_constructor.explore_dependencies()

        self.info_cleaner = InfoCleaner(
            possible_flags=self.info_gatherer.possible_flags,
            fails=self.info_gatherer.fails,
            flag_fails=self.info_gatherer.flag_fails,
            parents=self.dependency_constructor.parents,
            n_children=self.dependency_constructor.n_children,
            versions_by_universal_name=self.info_gatherer.versions_by_universal_name,
        )
        self.info_cleaner.clean()

        no_resolution = False
        for main_project, config, loc_entry, flags in self.main_projects:
            if main_project not in self.info_cleaner.possible_flags:
                logger.info(
                    f"The available packages were insufficient for the dependencies resolution of the package {config.metadata.name} at {loc_entry.path!s}."
                )
                no_resolution = True
            else:
                failed_flags: list[str] = []
                for flag in flags:
                    if isinstance(flag, str) and flag not in self.info_cleaner.possible_flags[main_project]:
                        failed_flags.append(flag)
                if len(failed_flags) > 0:
                    logger.info(
                        f"The available packages were insuffiicient for the dependency resolution for the following flags: {failed_flags} for the package {config.metadata.name} at {loc_entry.path!s}."
                    )
                    no_resolution = True

        if no_resolution:
            raise QuackPackError(reason=NoResolutionError())

        self.solver_engine = SolverEngine(
            gathered_configs=self.info_gatherer.gathered_configs,
            versions_by_universal_name=self.info_cleaner.versions_by_universal_name,
            possible_flags=self.info_cleaner.possible_flags,
        )
        self.solver_engine.run_engine([
            (main_project, flags) for main_project, _, _, flags in self.main_projects
        ])

        # TODO: If no resolution was found raise error.

        self.build_rules_constructor = BuildRulesConstructor(
            gathered_configs=self.info_gatherer.gathered_configs,
            variables_dep_version=self.solver_engine.variables_dep_version,
            variables_dep_flag=self.solver_engine.variables_dep_flag,
            variables_bare=self.solver_engine.variables_bare,
            variables_flag=self.solver_engine.variables_flag,
            model=self.solver_engine.model,
        )
        self.build_rules_constructor.construct_instructions()

        return (self.build_rules_constructor.all_flags, self.build_rules_constructor.instructions)
