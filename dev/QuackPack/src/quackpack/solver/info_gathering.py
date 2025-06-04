# TODO: Use different logic for pinning versions of dependencies.
import asyncio
from typing import cast

from quackpack.config.project import DependencyEntry, GitEntry, LocalEntry, Manifest, VersionList
from quackpack.fetcher.api_types import MultiMetadataResult, SingleGitMetadataResult
from quackpack.fetcher.fetcher import Fetcher
from quackpack.project_loader import Project
from quackpack.solver.universal_names import UniversalName
from quackpack.solver.util import NoFlags, SolverPackage, SolverPackageWithFlag, check_conditions, get_flags
from quackpack.util.version import Version

type LocalFetchResult = tuple[LocalEntry, Manifest | None]
type GatheredResult = tuple[SolverPackage, Manifest]


async def get_config_local(location: LocalEntry) -> LocalFetchResult:  # noqa: RUF029
    try:
        project = Project(manifest_path=location.path)
        config = project.manifest_with_acquiring_lock()
        return (location, config)
    except FileNotFoundError:
        return (location, None)


class InfoGatherer:
    # Inputs
    # ------
    fetcher: Fetcher
    # Fetcher instance.

    main_projects: list[tuple[SolverPackage, Manifest, LocalEntry, list[NoFlags | str]]]
    # List of the packages for which we want to resolve dependencies.
    # The single element is of form (package, its configuration, its local_entry, flags with which it is activated).
    # TODO: Are those packages always local? (maybe we would like somehow be resolving dependencies on a server?)

    # Byproducts
    # ----------
    requested: set[UniversalName]
    # Stores which git\local packages or lists of packages with the same name from server were requested.

    requested_what: dict[UniversalName, dict[Version, list[str | NoFlags]]]
    # Stores for a give package name which was requested from the server, what versions of that package triggered requests and with what flags.
    # This is used to find what versions are compatible with the requested version and to initiate info gathering from them.

    explored_flags: dict[SolverPackage, set[str | NoFlags]]
    # Stores for a given package what of its flags have been already explored.

    to_explore_flags: dict[SolverPackage, set[str | NoFlags]]
    # Stores for a given package what of its flags should be explored.

    # Outputs
    # -------
    gathered_configs: dict[SolverPackage, Manifest]
    # Mapping package -> its configuration.

    versions_by_universal_name: dict[UniversalName, list[Version | GitEntry | LocalEntry]]
    # Mapping universal name -> all of its gathered versions.

    possible_flags: dict[SolverPackage, set[NoFlags | str]]
    # All of the packages which may be used in the dependency solution with all of the flags which may be used.

    fails: set[UniversalName]
    # Package names, for which fetching failed.

    flag_fails: set[SolverPackageWithFlag]
    # Flags which were expected to be exposed by the given package but were not.

    def __init__(
        self,
        fetcher: Fetcher,
        main_projects: list[tuple[SolverPackage, Manifest, LocalEntry, list[NoFlags | str]]],
    ):
        self.main_projects = main_projects
        self.fetcher = fetcher
        self.gathered_configs = {}
        self.requested = set()
        self.requested_what = {}
        self.explored_flags = {}
        self.to_explore_flags = {}
        self.versions_by_universal_name = {}
        self.P_main = {}
        self.possible_flags = {}
        self.flag_fails = set()
        self.fails = set()

    async def get_config(self, package: SolverPackage) -> tuple[SolverPackage, Manifest]:
        """
        Falsely asynchronous function to read the configuration from self.gathered_configs.

        Args:
            package (SolverPackage): Package for which the configuration should be returned.

        Returns:
            tuple[SolverPackage, Manifest]: Pair (package, its configuration).
        """
        return (package, self.gathered_configs[package])

    def check_single_version(
        self,
        dep_uni_name: UniversalName,
        version: Version | GitEntry | LocalEntry,
        flags_to_check: list[NoFlags | str],
    ) -> bool:
        """
        Checks if any flag from flags_to_check has not yet been explored.
        If so, it is added to to_explore_flags.

        Args:
            dep_uni_name (UniversalName): Package name, for which we apply the check.
            version (Version | GitEntry | LocalEntry): Version of the package.
            flags_to_check (list[NoFlags  |  str]): List of flags to check.

        Returns:
            bool: Whether there are new flags to explore or not.
        """
        dep_package = SolverPackage(dep_uni_name, version)
        new_flags = False

        for flag in flags_to_check:
            if (dep_package in self.explored_flags and flag in self.explored_flags[dep_package]) or (
                dep_package in self.to_explore_flags and flag in self.to_explore_flags[dep_package]
            ):
                continue

            if dep_package not in self.to_explore_flags:
                self.to_explore_flags[dep_package] = set()
            self.to_explore_flags[dep_package].add(flag)
            new_flags = True

        return new_flags

    def explore_single_dependency(
        self, origin_flag: str | NoFlags, dep_uni_name: UniversalName, dep_entry: DependencyEntry
    ) -> list[
        asyncio.Task[MultiMetadataResult | SingleGitMetadataResult | GatheredResult | LocalFetchResult]
    ]:
        """
        Explores single dependency entry, returning the list of new tasks to be done.

        Args:
            origin_flag (str | NoFlags): Flag of the parent package for which we explore this dependency.
            dep_uni_name (UniversalName): Universal name of the dependency.
            dep_entry (DependencyEntry): Dependency entry describing the dependency.

        Returns:
            list[asyncio.Task[MultiMetadataResult | SingleGitMetadataResult | GatheredResult | LocalFetchResult]]: List of the new tasks.
        """
        if not check_conditions(origin_flag, dep_entry.conditions) or dep_uni_name in self.fails:
            return []

        tasks: list[
            asyncio.Task[MultiMetadataResult | SingleGitMetadataResult | GatheredResult | LocalFetchResult]
        ] = []

        # Find which flags should be explored further.
        flags_to_check = get_flags(origin_flag=origin_flag, dep_entry=dep_entry)

        if isinstance(dep_entry.version, (LocalEntry, GitEntry)) and self.check_single_version(
            dep_uni_name=dep_uni_name, version=dep_entry.version, flags_to_check=flags_to_check
        ):
            # We have a local/git dependency to explore with some not yet explored flags.
            if dep_uni_name not in self.versions_by_universal_name and dep_uni_name not in self.requested:
                # This dependency has not yet been fetched nor requested.
                self.requested.add(dep_uni_name)
                if isinstance(dep_entry.version, LocalEntry):
                    tasks.append(asyncio.create_task(get_config_local(dep_entry.version)))
                else:
                    tasks.append(asyncio.create_task(self.fetcher.clone_from_git(dep_entry.version)))

            elif dep_uni_name in self.versions_by_universal_name:
                # The dependency has been fetched, so we add the simple task of reading its config.
                tasks.append(
                    asyncio.create_task(
                        self.get_config(package=SolverPackage(name=dep_uni_name, version=dep_entry.version))
                    )
                )
            # If the dependency has been requested but not yet fetched we do nothing,
            # because check_single_version has already added the necessary flags.

        elif isinstance(dep_entry.version, VersionList):
            assert dep_uni_name.server_url is not None
            if dep_uni_name not in self.versions_by_universal_name:
                # The list of configs of all packages with this name has not yet been fetched, so we add info which packages requested what flags,
                # to serve appropriately when the fetching succeeds.
                if dep_uni_name not in self.requested_what:
                    self.requested_what[dep_uni_name] = {}
                for version in dep_entry.version:
                    if version not in self.requested_what[dep_uni_name]:
                        self.requested_what[dep_uni_name][version] = []
                    self.requested_what[dep_uni_name][version] += flags_to_check

                if dep_uni_name not in self.requested:
                    # If the package name has not yet been requested, we request it.
                    self.requested.add(dep_uni_name)
                    tasks.append(
                        asyncio.create_task(
                            self.fetcher.get_package_all_metadata(
                                instance_url=dep_uni_name.server_url, package_name=dep_uni_name.name
                            )
                        )
                    )
            else:
                # We already have configs of all the packages with such name.
                for potential_version in self.versions_by_universal_name[dep_uni_name]:
                    assert isinstance(potential_version, Version)
                    for version in dep_entry.version:
                        if version.can_be_upgraded_to(potential_version) and self.check_single_version(
                            dep_uni_name=dep_uni_name,
                            version=potential_version,
                            flags_to_check=flags_to_check,
                        ):
                            # The potential version is compatible with some version on the list and there are new flags to explore,
                            # which have been added to flags_to_explore by check_single_version.
                            tasks.append(
                                asyncio.create_task(
                                    self.get_config(
                                        SolverPackage(name=dep_uni_name, version=potential_version)
                                    )
                                )
                            )
        return tasks

    def explore_single_package(
        self, package: SolverPackage
    ) -> list[
        asyncio.Task[MultiMetadataResult | SingleGitMetadataResult | GatheredResult | LocalFetchResult]
    ]:
        """
        Explores dependencies of a single package with all the needed but yet unexplored of its flags.

        Args:
            package (SolverPackage): The package.

        Returns:
            list[asyncio.Task[MultiMetadataResult | SingleGitMetadataResult | GatheredResult | LocalFetchResult]]: The new tasks to be done.
        """
        tasks: list[
            asyncio.Task[MultiMetadataResult | SingleGitMetadataResult | GatheredResult | LocalFetchResult]
        ] = []

        config = self.gathered_configs[package]

        for flag in self.to_explore_flags[package]:
            if package not in self.explored_flags:
                self.explored_flags[package] = set()
            self.explored_flags[package].add(flag)
            if package not in self.possible_flags:
                self.possible_flags[package] = set()
            self.possible_flags[package].add(flag)
            if flag is NoFlags.NO_FLAGS or flag in config.features:
                for dep_name, dep_entry in config.dependencies.items():
                    tasks += self.explore_single_dependency(
                        flag,
                        UniversalName.create_from_dep_entry(dep_name=dep_name, dep_entry=dep_entry),
                        dep_entry,
                    )
            else:
                # TODO: With not allow_unusable this should raise an error.
                self.flag_fails.add(SolverPackageWithFlag(package=package, flag=flag))
        self.to_explore_flags[package] = set()

        return tasks

    def serve_fetched_multimetadata(
        self, fetch_result: MultiMetadataResult
    ) -> list[
        asyncio.Task[MultiMetadataResult | SingleGitMetadataResult | GatheredResult | LocalFetchResult]
    ]:
        """
        Initiates info gathering on the results of a multimetadata request to a server.

        Args:
            fetch_result (MultiMetadataResult): Result of the fetch.

        Returns:
            list[asyncio.Task[MultiMetadataResult | SingleGitMetadataResult | GatheredResult | LocalFetchResult]]: The list of ne tasks to be done.
        """
        tasks: list[
            asyncio.Task[MultiMetadataResult | SingleGitMetadataResult | GatheredResult | LocalFetchResult]
        ] = []

        uni_name = UniversalName.server_uni_name(
            name=fetch_result.package_name, server_url=fetch_result.instance_url
        )
        self.versions_by_universal_name[uni_name] = []
        if fetch_result.result is None:
            # The fetch failed.
            self.fails.add(uni_name)
        else:
            # Update gathered_configs and version_by_universal_name
            for configuration in fetch_result.result.packages_metadata:
                version = configuration.metadata.version
                package = SolverPackage(name=uni_name, version=version)
                self.gathered_configs[package] = configuration
                self.versions_by_universal_name[uni_name].append(version)

                for requested_version, new_flags in self.requested_what[uni_name].items():
                    if version.can_be_upgraded_to(requested_version) and self.check_single_version(
                        dep_uni_name=uni_name, version=version, flags_to_check=new_flags
                    ):
                        tasks.append(
                            asyncio.create_task(
                                self.get_config(SolverPackage(name=uni_name, version=version))
                            )
                        )

        # Clean requested and requested_what
        self.requested.remove(uni_name)
        self.requested_what[uni_name] = {}

        return tasks

    def serve_fetched_local(
        self, fetch_result: LocalFetchResult
    ) -> list[
        asyncio.Task[MultiMetadataResult | SingleGitMetadataResult | GatheredResult | LocalFetchResult]
    ]:
        """
        Initiates info gathering on the results of a local fetch.

        Args:
            fetch_result (LocalFetchResult): Result of the fetch.

        Returns:
            list[asyncio.Task[MultiMetadataResult | SingleGitMetadataResult | GatheredResult | LocalFetchResult]]: The list of new tasks to be done.
        """
        tasks: list[
            asyncio.Task[MultiMetadataResult | SingleGitMetadataResult | GatheredResult | LocalFetchResult]
        ] = []

        entry = fetch_result[0]
        configuration = fetch_result[1]
        uni_name = UniversalName.local_uni_name(entry.path)
        package = SolverPackage(name=uni_name, version=entry)

        if configuration is None:
            self.fails.add(uni_name)
            return tasks

        self.versions_by_universal_name[uni_name] = [entry]
        self.gathered_configs[package] = configuration
        return self.explore_single_package(package)

    def serve_fetched_git(
        self, fetch_result: SingleGitMetadataResult
    ) -> list[
        asyncio.Task[MultiMetadataResult | SingleGitMetadataResult | GatheredResult | LocalFetchResult]
    ]:
        """
        Initiates info gathering on the results of a git fetch.

        Args:
            fetch_result (SingleGitMetadataResult): Result of the fetch.

        Returns:
            list[asyncio.Task[MultiMetadataResult | SingleGitMetadataResult | GatheredResult | LocalFetchResult]]: The list of new tasks to be done.
        """
        tasks: list[
            asyncio.Task[MultiMetadataResult | SingleGitMetadataResult | GatheredResult | LocalFetchResult]
        ] = []

        git_entry = fetch_result.git_entry
        uni_name = UniversalName.git_uni_name(git_entry)
        configuration = fetch_result.result
        package = SolverPackage(name=uni_name, version=git_entry)

        if configuration is None:
            self.fails.add(uni_name)
            return tasks

        self.versions_by_universal_name[uni_name] = [git_entry]
        self.gathered_configs[package] = configuration
        return self.explore_single_package(package)

    def serve_gathered(
        self, result: GatheredResult
    ) -> list[
        asyncio.Task[MultiMetadataResult | SingleGitMetadataResult | GatheredResult | LocalFetchResult]
    ]:
        """
        Initiates info gathering on the results of a dummy fetch to gathered_configs.

        Args:
            result (GatheredResult): The result of the fetch.

        Returns:
            list[asyncio.Task[MultiMetadataResult | GatheredResult | LocalFetchResult]]: The list of new tasks to be done.
        """
        return self.explore_single_package(result[0])

    def serve_finished_task(
        self,
        done_task: asyncio.Task[
            MultiMetadataResult | SingleGitMetadataResult | GatheredResult | LocalFetchResult
        ],
    ) -> list[
        asyncio.Task[MultiMetadataResult | SingleGitMetadataResult | GatheredResult | LocalFetchResult]
    ]:
        """
        Aggregates serving all kinds of the fetches.

        Args:
            done_task (asyncio.Task[MultiMetadataResult  |  GatheredResult  |  LocalFetchResult]): The result of the fetch.

        Returns:
            list[asyncio.Task[MultiMetadataResult | GatheredResult | LocalFetchResult]]: The list of new tasks to be done.
        """
        result = done_task.result()
        if isinstance(result, MultiMetadataResult):
            return self.serve_fetched_multimetadata(result)
        elif isinstance(result, SingleGitMetadataResult):
            return self.serve_fetched_git(result)
        assert isinstance(result, tuple)
        if isinstance(result[0], SolverPackage):
            return self.serve_gathered(result=cast(GatheredResult, result))
        else:
            return self.serve_fetched_local(fetch_result=cast(LocalFetchResult, result))

    async def gather(self) -> None:
        """
        Main entry point.
        Initiates dummy fetches for all of the main projects.
        """
        for main_project, main_config, main_version, main_flags in self.main_projects:
            self.gathered_configs[main_project] = main_config
            self.to_explore_flags[main_project] = set()
            self.versions_by_universal_name[main_project.name] = [main_version]
            self.to_explore_flags[main_project] = set(main_flags)

        tasks: list[
            asyncio.Task[MultiMetadataResult | SingleGitMetadataResult | GatheredResult | LocalFetchResult]
        ] = [
            asyncio.create_task(self.get_config(main_project)) for main_project, _, _, _ in self.main_projects
        ]

        while tasks:
            done, pending = await asyncio.wait(tasks, return_when=asyncio.FIRST_COMPLETED)
            tasks = list(pending)

            for done_task in done:
                tasks += self.serve_finished_task(done_task=done_task)
