"""
Provides high level storage operations. Access control is provided by ``locks``
module. Operations for state modification, which preserve coherency, are
provided by ``files`` module.
"""

import asyncio
import shutil
import tarfile
from pathlib import Path
from time import time

from pydantic import AnyUrl

from quackpack.compile import CodeSink, Continuation
from quackpack.fetcher.api_types import Package
from quackpack.fetcher.fetcher import Fetcher
from quackpack.project import Project
from quackpack.signals import EnableInterrupt, SignalInterrupt
from quackpack.solver.solver import Solver
from quackpack.solver.util import SolverPackage
from quackpack.storage.files import Dependency, StorageVenv, fix_and_load_venv, save_venv, try_fsync_dir
from quackpack.storage.locks import CleanLock, RunLock, TrySyncLock, cleanup_locks
from quackpack.storage.paths import StoragePaths
from quackpack.util.errors import QuackPackError
from quackpack.util.global_context import GlobalContext
from quackpack.util.lock import FileLock
from quackpack.util.pkgid import Identifier, ResolvedId, ResolvedIdDucknest, ResolvedIdLocal
from quackpack.util.version import Version


def run(
    ctx: GlobalContext, id_or_project: Project | Identifier, sink: CodeSink, entry_path: Path
) -> Continuation:
    """
    Run code or compile it, depending on the provided ``sink``.

    :param quackpack.util.global_context.GlobalContext ctx: Global configuration context.
    :param quackpack.util.pkgid.Identifier | quackpack.project.Project id_or_project: Configuration of the virtual environment.
    :param quackpack.storage.code_sink.CodeSink sink: Consumer responsible for code execution or compilation.
    :param pathlib.Path entry_path: Path to the main script file.
    :return: This function does not return.
    :rtype: Never
    :raises quackpack.util.errors.QuackPackError: If the virtual environment does not exist.
    """

    storage = StoragePaths(ctx.configuration.storage.storage)
    venv_id = (
        id_or_project
        if isinstance(id_or_project, Identifier)
        else id_or_project.manifest_with_acquiring_lock(enable_robust_handler=False).metadata.name
    )

    connection = sink.connect()

    def load_libraries(dependencies: dict[Identifier, Dependency]) -> None:
        for alias, dep in dependencies.items():
            if connection.check_platform(dep.system, dep.arch):
                # TODO enable interrupt??? (lub nie, może implementacja `load_library` to zrobi)
                connection.load_library(alias, dep.id, storage.pkg_dir(dep.id))

    with RunLock(storage, venv_id):
        with FileLock(storage.venv_data_lock(venv_id)):
            data = fix_and_load_venv(storage, venv_id)
            if data is None:
                raise QuackPackError(f"the virtual environment {venv_id} does not exist")
            if isinstance(id_or_project, Project):
                # TODO 🔨 check if venv is synchronized
                ...
            data.last_access = time()
            save_venv(storage, venv_id, data)
            # If the venv is temporary, we load dependencies under data lock:
            # otherwise concurrent clean operation could have deleted the data
            # file and its dependencies, so `RunLock` does not suffice to
            # guarantee persistence of required libraries.
            if data.metadata.is_temporary:
                load_libraries(data.dependencies)
        # In case of non-temporary venv, we load libraries outsize of data lock,
        # to hold it for as short as possible.
        if not data.metadata.is_temporary:
            load_libraries(data.dependencies)

    return connection.finalize(entry_path)


def ugly_glue(pkg: SolverPackage, flags: list[str]) -> Dependency:
    """
    Ugly docstring.
    """

    name = pkg.name.name
    id: ResolvedId | None = None
    if pkg.name.server_url is not None:
        assert isinstance(pkg.version, Version)
        id = ResolvedIdDucknest(id=name, version=pkg.version, url=pkg.name.server_url).upcast()
    if pkg.name.git_entry is not None:
        raise NotImplementedError("TODO solver")
    if pkg.name.local_path is not None:
        id = ResolvedIdLocal(path=pkg.name.local_path).upcast()
    assert id is not None
    # TODO system and arch
    return Dependency(id=id, used_flags=flags, system=None, arch=None)


def venv_sync(ctx: GlobalContext, project: Project) -> None:
    """
    Synchronize the storage state of a virtual environment with the user's project configuration.

    :param quackpack.util.global_context.GlobalContext ctx: Global configuration context.
    :param quackpack.project.Project project: Project configuration to synchronize with.
    :return: None
    :rtype: None
    """

    storage = StoragePaths(ctx.configuration.storage.storage)
    manifest = project.manifest_with_acquiring_lock(enable_robust_handler=False)
    venv_id = manifest.metadata.name
    with TrySyncLock(storage, venv_id):
        # TODO solver will need to convert to using Identifier as flags
        solver = Solver([(project.manifest_path, list(map(str, manifest.features.keys())))])
        with FileLock(ctx.configuration.cache.fetcher_lockfile):
            # TODO allow_unusable
            asyncio.run(solver.gather(Fetcher(ctx), allow_unusable=False))
        # TODO save the dependencies and do something with that
        # TODO aliases in solver
        dep_flags_dict, _ = solver.finish_solving()
        new_deps = {
            pkg.name.name: ugly_glue(pkg, flags)
            for pkg, flags in dep_flags_dict.items()
            if pkg.name.name != "NONE"
        }

        with FileLock(ctx.configuration.cache.fetcher_lockfile):
            to_install = [
                dep.id.inner
                # TODO some other check if pkg is correctly installed
                # and if not, overwrite it in the installation?
                for dep in new_deps.values()
                if isinstance(dep.id.inner, ResolvedIdDucknest) and not storage.pkg_dir(dep.id).exists()
            ]
            try:
                with EnableInterrupt():
                    asyncio.run(_fetch_blobs(storage, Fetcher(ctx), to_install))
            except SignalInterrupt:
                # TODO probably some cleanup for package directories left in bad state?
                pass
        # If the operation fails after installing some packages, nothing bad happens;
        # they will be removed in the next clean operation if they remain orphaned.

        with FileLock(storage.venv_data_lock(venv_id)):
            data = fix_and_load_venv(storage, venv_id)
            if (
                data is not None
                and project.manifest_path != data.last_location
                and data.last_location.exists()
            ):
                try:
                    replaces = (
                        Project(data.last_location)
                        .manifest_with_acquiring_lock(enable_robust_handler=False)
                        .metadata.name
                        == venv_id
                    )
                except QuackPackError:
                    replaces = False
                if replaces:
                    # TODO 🔨 Wypada też dać flagę `--force` i poinformować użytkownika że jest taka.
                    raise QuackPackError("overwriting existing virtual environment from another location")
            timestamp = time()
            save_venv(
                storage,
                venv_id,
                StorageVenv(
                    dependencies=new_deps,
                    metadata=manifest.metadata,
                    last_location=project.manifest_path,
                    last_modification=timestamp,
                    last_access=timestamp,
                ),
            )


async def _fetch_blobs(storage: StoragePaths, fetcher: Fetcher, to_install: list[ResolvedIdDucknest]):
    """
    Fetch and extract package blobs required for installation.

    :param quackpack.storage.paths.StoragePaths storage: Storage access paths.
    :param quackpack.fetcher.fetcher.Fetcher fetcher: Fetcher used for retrieving packages.
    :param list[quackpack.util.pkgid.ResolvedIdDucknest] to_install: List of package identifiers to fetch and extract.
    :return: None
    :rtype: None
    """

    with fetcher as fetcher:

        def untar(src_path: Path, pkg_id: ResolvedIdDucknest):
            pkg_dir = storage.pkg_dir(pkg_id.upcast())
            # TODO some other check if pkg is correctly installed
            # and if not, overwrite it in the installation?
            if not pkg_dir.exists():
                pkg_dir.mkdir(parents=True, exist_ok=True)
                with tarfile.open(src_path, "r:gz") as tar:
                    tar.extractall(path=pkg_dir)
                # TODO probably add some completion marker?
                try_fsync_dir(pkg_dir)

        async def fetch_task(pkg: ResolvedIdDucknest):
            # TODO nieskończona pętla, huuuuuura!!!
            blob_path = None
            while blob_path is None:
                blob_path = await fetcher.get_package_blob(
                    AnyUrl(pkg.url), Package(id=pkg.id, version=str(pkg.version))
                )
            await asyncio.to_thread(untar, *(blob_path, pkg))

        await asyncio.gather(*(asyncio.create_task(fetch_task(pkg)) for pkg in to_install))


def venv_delete(ctx: GlobalContext, id_or_project: Project | Identifier) -> None:
    """
    Delete a virtual environment from storage.

    :param quackpack.util.global_context.GlobalContext ctx: Global configuration context.
    :param quackpack.util.pkgid.Identifier | quackpack.project.Project id_or_project: Identifier or project associated with the virtual environment to delete.
    :return: None
    :rtype: None
    """

    storage = StoragePaths(ctx.configuration.storage.storage)
    venv_id = (
        id_or_project
        if isinstance(id_or_project, Identifier)
        else id_or_project.manifest_with_acquiring_lock(enable_robust_handler=False).metadata.name
    )
    with TrySyncLock(storage, venv_id), FileLock(storage.venv_data_lock(venv_id)), EnableInterrupt():
        shutil.rmtree(storage.venv_dir(venv_id))


def clean_storage(ctx: GlobalContext) -> None:
    """
    Remove orphaned packages and expired temporary virtual environments from storage.

    :param quackpack.util.global_context.GlobalContext ctx: Global configuration context.
    :return: None
    :rtype: None
    """

    storage = StoragePaths(ctx.configuration.storage.storage)
    with CleanLock(storage):
        all_deps: set[str] = set()
        for venv in storage.iter_venvs():
            venv_id = Identifier(venv.name)
            with FileLock(storage.venv_data_lock(venv_id)):
                data = fix_and_load_venv(storage, venv_id)
                if data is None:
                    continue
                # TODO 🔨 think about system time changes
                if (
                    data.metadata.is_temporary
                    and data.last_access + ctx.configuration.storage.temporary_lifetime < time()
                ):
                    with EnableInterrupt():
                        shutil.rmtree(venv)
                    continue
                all_deps |= {dep.id.dir_name() for dep in data.dependencies.values()}

        cleanup_locks(storage)

        # Removing those packages is safe, as no existing venv has a reference
        # to any of those, and there cannot appear any new one that does,
        # as that is blocked by holding `CleanLock`.
        for pkg in storage.iter_pkgs():
            if pkg.name not in all_deps:
                with EnableInterrupt():
                    shutil.rmtree(pkg)


def venv_list(ctx: GlobalContext) -> dict[str, StorageVenv]:
    """
    Get a snapshot of all virtual environments' states.

    The combined state may never have existed in storage as a consistent whole; this function locks each
    virtual environment separately. Equivalent to calling :func:`venv_info` on all virtual environments present
    in the storage.

    :param quackpack.util.global_context.GlobalContext ctx: Global configuration context.
    :return: A mapping of virtual environment names to their stored state.
    :rtype: dict[str, quackpack.storage.files.StorageVenv]
    """

    storage = StoragePaths(ctx.configuration.storage.storage)
    metadata: dict[str, StorageVenv] = {}
    for venv in storage.iter_venvs():
        venv_id = Identifier(venv.name)
        with EnableInterrupt():
            pass
        # Note on the possibility of TOCTOU: this check does not add a guarantee
        # that we will not lock a no-longer existing venv data lock.
        if not venv.is_dir():
            continue
        with FileLock(storage.venv_data_lock(venv_id)):
            data = fix_and_load_venv(storage, venv_id)
            if data is not None:
                metadata[venv.name] = data
    return metadata


def venv_info(ctx: GlobalContext, id_or_project: Project | Identifier) -> StorageVenv | None:
    """
    Retrieve the storage state of a specific virtual environment.

    :param quackpack.util.global_context.GlobalContext ctx: Global configuration context.
    :param quackpack.util.pkgid.Identifier | quackpack.project.Project id_or_project: Identifier or project of the virtual environment.
    :return: Storage state of the virtual environment, or None if not found.
    :rtype: quackpack.storage.files.StorageVenv | None
    """

    storage = StoragePaths(ctx.configuration.storage.storage)
    venv_id = (
        id_or_project
        if isinstance(id_or_project, Identifier)
        else id_or_project.manifest_with_acquiring_lock(enable_robust_handler=False).metadata.name
    )
    with FileLock(storage.venv_data_lock(venv_id)):
        data = fix_and_load_venv(storage, venv_id)
    return data
