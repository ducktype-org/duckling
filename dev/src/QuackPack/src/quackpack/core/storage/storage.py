"""
Provides high level storage operations. Access control is provided by ``locks``
module. Operations for state modification, which preserve coherency, are
provided by ``files`` module.
"""

import asyncio
import shutil
import tarfile
from dataclasses import dataclass
from pathlib import Path
from time import time
from typing import Final

from pydantic import AnyUrl, ValidationError
from rich import live
from rich.progress import Progress

from quackpack.compile import CodeSink, Continuation
from quackpack.core.fetcher.api_types import Package as FetcherPackage
from quackpack.core.fetcher.fetcher import FetcherContext
from quackpack.core.package_loader import PackageLoader
from quackpack.core.signals import EnableInterrupt
from quackpack.core.solver.solver import Solver
from quackpack.core.solver.types.solver_mode import SolverMode
from quackpack.core.storage.files import StorageVenv, VenvFreeze, fix_and_load_venv, save_venv, try_fsync_dir
from quackpack.core.storage.git_access import StorageGitAccess
from quackpack.core.storage.locks import CleanLock, RunLock, TrySyncLock, cleanup_locks
from quackpack.core.storage.paths import StoragePaths
from quackpack.core.types.manifest.source import GitSource, SourceKind
from quackpack.core.types.manifest.summary import Summary
from quackpack.core.types.package import Package
from quackpack.util.global_context import GlobalContext
from quackpack.util.lock import FileLock
from quackpack.util.lock.common import LockWouldBlock
from quackpack.util.logger import get_logger
from quackpack.util.types.errors import QuackPackError
from quackpack.util.types.pkgid import GitPackageId, Identifier, LocalPackageId, PackageId, RegistryPackageId

logger = get_logger(__name__)


MAX_BLOB_RETRY_COUNT: Final[int] = 3


def run(
    ctx: GlobalContext, id_or_package: Package | Identifier, sink: CodeSink, entry_path: Path
) -> Continuation:
    """
    Run code or compile it, depending on the provided ``sink``.

    :param quackpack.util.global_context.GlobalContext ctx: Global configuration context.
    :param quackpack.util.pkgid.Identifier | quackpack.package.Package id_or_package: Configuration of the virtual environment.
    :param quackpack.storage.code_sink.CodeSink sink: Consumer responsible for code execution or compilation.
    :param pathlib.Path entry_path: Path to the main script file.
    :return: Continuation returned by the CodeSink.
    :rtype: Continuation
    :raises quackpack.util.errors.QuackPackError: If the virtual environment does not exist.
    """
    storage = StoragePaths(ctx.ensure_storage_dir())
    manifest, venv_id = (
        (manifest := id_or_package.manifest, manifest.summary.name)
        if isinstance(id_or_package, Package)
        else (None, id_or_package)
    )
    input_freeze: VenvFreeze | None = None
    if isinstance(id_or_package, Package):
        try:
            input_freeze = _load_external_freezefile(id_or_package)
        except CorruptedFreezefile:
            raise QuackPackError("Corrupted freeze file") from None

    connection = sink.connect()

    with RunLock(storage, venv_id):
        with FileLock(storage.venv_data_lock(venv_id)):
            data = fix_and_load_venv(storage, venv_id)
            if data is None:
                raise QuackPackError(f"the virtual environment {venv_id} does not exist")
            if isinstance(id_or_package, Package):
                summary = None
                if manifest is not None:
                    summary = manifest.summary
                if not is_synchronized(
                    summary, input_freeze, data, id_or_package.venv_config.freezefile_exposed()
                ):
                    raise QuackPackError("Venv is unsynchronized")
            data.last_access = time()
            save_venv(storage, venv_id, data)

            path_mappings = _dependency_paths(storage, data)

            # If the venv is temporary, we load dependencies under data lock:
            # otherwise concurrent clean operation could have deleted the data
            # file and its dependencies, so `RunLock` does not suffice to
            # guarantee persistence of required libraries.
            if data.is_ephemeral:
                connection.load_dependencies(data.freeze, path_mappings)

        # In case of non-temporary venv, we load libraries outsize of data lock,
        # to hold it for as short as possible.
        if not data.is_ephemeral:
            connection.load_dependencies(data.freeze, path_mappings)

    return connection.finalize(entry_path)


def _dependency_paths(storage: StoragePaths, data: StorageVenv) -> dict[PackageId, Path]:
    result = dict[PackageId, Path]()
    for id in data.freeze.dependencies:
        if isinstance(id.inner, LocalPackageId):
            result[id] = id.inner.path if id.inner.path.is_absolute() else data.last_location / id.inner.path
        else:
            result[id] = storage.pkg_dir(id)
    return result


def venv_sync(
    ctx: GlobalContext, package: Package, overwrite: bool = False, frozen: bool = False, offline: bool = False
) -> None:
    """
    Synchronize the storage state of a virtual environment with the user's package configuration.

    :param quackpack.util.global_context.GlobalContext ctx: Global configuration context.
    :param quackpack.package.Package package: Package configuration to synchronize with.
    :return: None
    :rtype: None
    """
    storage = StoragePaths(ctx.ensure_storage_dir())
    manifest = package.manifest
    venv_config = package.venv_config
    input_freeze = _load_external_freezefile(package)
    venv_id = manifest.summary.name

    try:
        with TrySyncLock(storage, venv_id):
            with FileLock(storage.venv_data_lock(venv_id)):
                data = fix_and_load_venv(storage, venv_id)
            if is_synchronized(manifest.summary, input_freeze, data, venv_config.freezefile_exposed()):
                logger.debug("Already synchronized")
                return

            freeze: VenvFreeze | None = None
            if input_freeze is not None and check_direct_dependencies(manifest.summary, input_freeze):
                # TODO the external freezefile should probably be checked for correctness
                # (also note: currently when direct dependencies are not satisfied, the freeze file is regenerated,
                # maybe user should opt-in to that)
                freeze = input_freeze
            if freeze is None:
                if frozen:
                    raise QuackPackError("Synchronization would require changing the freezefile")
                solver = Solver(
                    ctx=ctx,
                    root_path=package.package_root,
                    root_summary=manifest.summary,
                    root_flags=set(manifest.summary.features.keys()),
                    # TODO what about MERCIFUL?
                    # also OFFLINE is not yet implemented in the solver
                    # and will require to morph (the nonexistent) GitPaths
                    # even further into some general storage package provider / aranger
                    mode=SolverMode.OFFLINE if offline else SolverMode.STRICT,
                )
                with FileLock(ctx.ensure_fetcher_lockfile()):
                    git_access = StorageGitAccess(
                        paths=storage, cached=data.freeze.git_fetch_cache if data is not None else {}
                    )
                    gathered_data = asyncio.run(solver.prepare_solving(git_access))
                freeze = solver.finish_solving(gathered_data)
                logger.debug(f"Solver returned freeze {freeze!s}")

            with FileLock(ctx.ensure_fetcher_lockfile()):
                to_install: list[PackageId] = [
                    dep
                    for dep in freeze.dependencies
                    if not dep.is_local() and not storage.package_stored(dep)
                ]
                logger.debug(f"Packages to install: {to_install!s}")
                with EnableInterrupt():
                    asyncio.run(_fetch_blobs(storage, FetcherContext(ctx), to_install))
            # If the operation fails after installing some packages, nothing bad happens;
            # they will be removed in the next clean operation if they remain orphaned.

            if (not overwrite) and (
                data is not None
                and package.manifest_path != data.last_location
                and data.last_location.exists()
            ):
                try:
                    replaces = Package(data.last_location, None, None, ctx).manifest.summary.name == venv_id
                except QuackPackError:
                    replaces = False
                if replaces:
                    raise QuackPackError(
                        "tried to overwrite existing virtual environment from another location. Use --overwrite flag to force overwrite"
                    )

            with FileLock(storage.venv_data_lock(venv_id)):
                timestamp = time()
                save_venv(
                    storage,
                    venv_id,
                    StorageVenv(
                        freeze=freeze,
                        original_schema=manifest.summary.into_schema(),
                        last_location=package.manifest_path,
                        last_modification=timestamp,
                        last_access=timestamp,
                        is_ephemeral=venv_config.is_ephemeral(),
                    ),
                )
            if venv_config.freezefile_exposed() and not frozen:
                _freezefile_path(package).write_text(freeze.model_dump_json())
    except LockWouldBlock:
        raise QuackPackError(
            "Another synchronization operation is ongoing in this virtual environment"
        ) from None


async def _fetch_blobs(storage: StoragePaths, fetcher_ctx: FetcherContext, to_install: list[PackageId]):
    """
    Fetch and extract package blobs required for installation.

    :param quackpack.storage.paths.StoragePaths storage: Storage access paths.
    :param quackpack.fetcher.fetcher.Fetcher fetcher: Fetcher used for retrieving packages.
    :param list[quackpack.util.pkgid.PackageId] to_install: List of package identifiers to fetch and extract.
    :return: None
    :rtype: None
    """
    with fetcher_ctx as fetcher:

        def untar(src_path: Path, pkg_id: RegistryPackageId) -> None:
            if not storage.package_stored(pkg_id.upcast()):
                pkg_dir = storage.pkg_dir(pkg_id.upcast())
                # The package was corrupted so we delete it.
                if pkg_dir.exists():
                    shutil.rmtree(pkg_dir)
                pkg_dir.mkdir(parents=True, exist_ok=True)
                with tarfile.open(src_path, "r:gz") as tar:
                    tar.extractall(path=pkg_dir)
                storage.add_checksum(pkg_id.upcast())
                try_fsync_dir(pkg_dir)

        async def fetch_task_registry(pkg: RegistryPackageId) -> None:
            retry_count = 0
            blob_path = None
            while blob_path is None and retry_count < MAX_BLOB_RETRY_COUNT:
                blob_path = await fetcher.get_package_blob(
                    str(pkg.url), FetcherPackage(id=pkg.id, version=str(pkg.version))
                )
                retry_count += 1
            if blob_path is None:
                raise QuackPackError(f"Could not fetch package source for {pkg!s}")
            await asyncio.to_thread(untar, *(blob_path, pkg))

        async def fetch_task_git(pkg_id: GitPackageId) -> None:
            pkg_dir = storage.pkg_dir(pkg_id.upcast())
            git_result = await fetcher.clone_from_git(GitSource(git_url=pkg_id.url, commit=pkg_id.commit))
            if pkg_dir.exists():
                shutil.rmtree(pkg_dir)
            git_result.destination_path.rename(pkg_dir)
            storage.add_checksum(pkg_id.upcast())
            try_fsync_dir(pkg_dir)

        with live.Live(fetcher.bundled_progress, transient=True):
            tasks_registry = (
                asyncio.create_task(fetch_task_registry(dep.inner))
                for dep in to_install
                if isinstance(dep.inner, RegistryPackageId)
            )
            tasks_git = (
                asyncio.create_task(fetch_task_git(dep.inner))
                for dep in to_install
                if isinstance(dep.inner, GitPackageId)
            )
            await asyncio.gather(*tasks_registry, *tasks_git)


def venv_delete(ctx: GlobalContext, id_or_package: Package | Identifier) -> None:
    """
    Delete a virtual environment from storage.

    :param quackpack.util.global_context.GlobalContext ctx: Global configuration context.
    :param quackpack.util.pkgid.Identifier | quackpack.package.package id_or_package: Identifier or package associated with the virtual environment to delete.
    :return: None
    :rtype: None
    """
    storage = StoragePaths(ctx.ensure_storage_dir())
    venv_id = id_or_package if isinstance(id_or_package, Identifier) else id_or_package.manifest.summary.name
    try:
        with TrySyncLock(storage, venv_id), FileLock(storage.venv_data_lock(venv_id)), EnableInterrupt():
            shutil.rmtree(storage.venv_dir(venv_id), ignore_errors=True)
    except LockWouldBlock:
        raise QuackPackError(
            "Another synchronization operation is ongoing in this virtual environment"
        ) from None


@dataclass
class CleanOutput:
    removed_venvs: list[Identifier]
    removed_packages: list[Path]


def clean_storage(ctx: GlobalContext) -> CleanOutput:
    """
    Remove orphaned packages and expired temporary virtual environments from storage.

    :param quackpack.util.global_context.GlobalContext ctx: Global configuration context.
    :return: None
    :rtype: None
    """
    storage = StoragePaths(ctx.ensure_storage_dir())
    removed_venvs: list[Identifier] = []
    with CleanLock(storage):
        all_deps: set[str] = set()
        venvs = list(storage.iter_venvs())
        ctx.console.info("Finding packages used by each virtual environment")
        with Progress(console=ctx.console) as progress:
            for venv in progress.track(venvs):
                venv_id = Identifier(venv.name)
                with FileLock(storage.venv_data_lock(venv_id)):
                    data = fix_and_load_venv(storage, venv_id)
                    if data is None:
                        continue
                    requires_save = False
                    current_time = time()
                    # last_access can exceed current_time only if there was a system time change.
                    # If ephemeral venv's previous last_access is far in the future, we may never
                    # clean it. Choosing to truncate the last_access to the present time may instead
                    # cause premature cleanups (when measured in real time), but that should
                    # not be problem for ephemeral venv.
                    if data.last_access > current_time:
                        data.last_access = current_time
                        requires_save = True
                    if data.is_ephemeral and data.last_access + ctx.storage_tmp_lifetime() < current_time:
                        logger.debug(
                            f"Removing expired ephemeral virtual environment at {venv} from the shared storage"
                        )
                        requires_save = False
                        removed_venvs.append(venv_id)
                        with EnableInterrupt():
                            shutil.rmtree(venv)
                        continue
                    if requires_save:
                        save_venv(storage, venv_id, data)
                    all_deps |= {
                        dep.storage_name()
                        for dep in data.freeze.dependencies
                        if not isinstance(dep, LocalPackageId)
                    }

        cleanup_locks(storage)

        # Removing those packages is safe, as no existing venv has a reference
        # to any of those, and there cannot appear any new one that does,
        # as that is blocked by holding `CleanLock`.

        removed_pkgs = [pkg for pkg in storage.iter_pkgs() if pkg.name not in all_deps]
        ctx.console.info("Removing unneeded packages")
        with Progress(console=ctx.console) as progress:
            for pkg in progress.track(removed_pkgs):
                logger.debug(f"Removing package at {pkg} from the shared storage")
                with EnableInterrupt():
                    shutil.rmtree(pkg)
        return CleanOutput(removed_venvs=removed_venvs, removed_packages=removed_pkgs)


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
    storage = StoragePaths(ctx.ensure_storage_dir())
    metadata: dict[str, StorageVenv] = {}
    for venv in storage.iter_venvs():
        venv_id = Identifier(venv.name)
        with EnableInterrupt():
            pass
        # Note on the possibility of TOCTOU: this check does not add a guarantee
        # that we will not lock a no-longer existing venv data lock,
        # but this is not a problem here.
        if not venv.is_dir():
            continue
        with FileLock(storage.venv_data_lock(venv_id)):
            data = fix_and_load_venv(storage, venv_id)
            if data is not None:
                metadata[venv.name] = data
    return metadata


def venv_info(ctx: GlobalContext, id_or_package: Package | Identifier) -> StorageVenv | None:
    """
    Retrieve the storage state of a specific virtual environment.

    :param quackpack.util.global_context.GlobalContext ctx: Global configuration context.
    :param quackpack.util.pkgid.Identifier | quackpack.package.Package id_or_package: Identifier or package of the virtual environment.
    :return: Storage state of the virtual environment, or None if not found.
    :rtype: quackpack.storage.files.StorageVenv | None
    """
    storage = StoragePaths(ctx.ensure_storage_dir())
    venv_id = id_or_package if isinstance(id_or_package, Identifier) else id_or_package.manifest.summary.name
    with FileLock(storage.venv_data_lock(venv_id)):
        data = fix_and_load_venv(storage, venv_id)
    return data


# TODO
# Recursive local dependencies should be checked,
# as they could have changed without storage knowledge
# (this is used in is_synchronized check, so we get false positives there).
# Also do something about system and arch (but that is completly wrong and ignored in the project as a whole)
def check_direct_dependencies(summary: Summary, freeze: VenvFreeze) -> bool:
    if len(freeze.direct_dependencies) != len(summary.deps):
        logger.debug("Direct dependencies check failed: lengths differ")
        return False
    for alias, dep in summary.deps.items():
        if alias not in freeze.direct_dependencies:
            logger.debug(f"Direct dependencies check failed: missing alias {alias}")
            return False
        freeze_dep_id = freeze.direct_dependencies[alias]
        forced_features = set(dep.enabled_features(summary.features))
        missing_features = forced_features.difference(freeze.dependencies[freeze_dep_id].used_flags)
        if missing_features:
            logger.debug(f"Direct dependencies check failed: features missing {missing_features}")
            return False
        if dep.is_pinned:
            if not isinstance(freeze_dep_id.inner, RegistryPackageId):
                logger.debug(f"Direct dependencies check failed on {freeze_dep_id}: expected registry id")
                return False
            if dep.real_name != freeze_dep_id.inner.id:
                logger.debug(
                    f"Direct dependencies check failed on {freeze_dep_id}: ids differ. Expected {dep.real_name}"
                )
                return False
            if dep.source.as_registry().registry_url != freeze_dep_id.inner.url:
                logger.debug(
                    f"Direct dependencies check failed on {freeze_dep_id}: registry urls differ. Expected {dep.source.as_registry().registry_url}"
                )
                return False
            if dep.versions[0] != freeze_dep_id.inner.version:
                logger.debug(
                    f"Direct dependencies check failed on {freeze_dep_id}: pinned versions differ. Expected dep.versions[0]"
                )
                return False
        else:
            match dep.source.kind():
                case SourceKind.Registry:
                    if not isinstance(freeze_dep_id.inner, RegistryPackageId):
                        logger.debug(
                            f"Direct dependencies check failed on {freeze_dep_id}: expected registry id"
                        )
                        return False
                    freeze_dep_id = freeze_dep_id.inner
                    if dep.real_name != freeze_dep_id.id:
                        logger.debug(
                            f"Direct dependencies check failed on {freeze_dep_id}: ids differ. Expected {dep.real_name}"
                        )
                        return False
                    registry_url = dep.source.as_registry().registry_url
                    # Use AnyUrl to ignore f.e. trailing slashes.
                    if AnyUrl(registry_url) != AnyUrl(freeze_dep_id.url):
                        logger.debug(
                            f"Direct dependencies check failed on {freeze_dep_id}: registry urls differ. Expected {registry_url}"
                        )
                        return False
                    if not any(ver.can_be_upgraded_to(freeze_dep_id.version) for ver in dep.versions):
                        logger.debug(
                            f"Direct dependencies check failed on {freeze_dep_id}: not upgradable from versions. Expected any of {dep.versions}"
                        )
                        return False
                case SourceKind.Git:
                    if not isinstance(freeze_dep_id.inner, GitPackageId):
                        logger.debug(f"Direct dependencies check failed on {freeze_dep_id}: expected git id")
                        return False
                    freeze_dep_id = freeze_dep_id.inner
                    # Use AnyUrl to ignore f.e. trailing slashes.
                    if AnyUrl(dep.source.as_git().git_url) != AnyUrl(freeze_dep_id.url):
                        logger.debug(
                            f"Direct dependencies check failed on {freeze_dep_id}: repository urls differ. Expected {dep.source.as_git().git_url}"
                        )
                        return False
                case SourceKind.Local:
                    if not isinstance(freeze_dep_id.inner, LocalPackageId):
                        logger.debug(
                            f"Direct dependencies check failed on {freeze_dep_id}: expected local id"
                        )
                        return False
                    freeze_dep_id = freeze_dep_id.inner
                    if dep.source.as_local().absolute_dir_root.resolve() != freeze_dep_id.path:
                        logger.debug(
                            f"Direct dependencies check failed on {freeze_dep_id}: local paths differ. Expected {dep.source.as_local().absolute_dir_root.resolve()}"
                        )
                        return False
    return True


def _freezefile_path(package: Package) -> Path:
    return package.package_root / PackageLoader.FREEZE_NAME


class CorruptedFreezefile(Exception):
    pass


def _load_external_freezefile(package: Package) -> VenvFreeze | None:
    if not package.venv_config.freezefile_exposed():
        return None
    freeze_path = _freezefile_path(package)
    if freeze_path.is_file():
        freeze_content = freeze_path.read_text()
        try:
            return VenvFreeze.model_validate_json(freeze_content)
        except ValidationError:
            raise CorruptedFreezefile from None
    return None


def get_freeze(ctx: GlobalContext, package: Package) -> VenvFreeze | None:
    storage = StoragePaths(ctx.ensure_storage_dir())
    venv_id = package.manifest.summary.name
    with FileLock(storage.venv_data_lock(venv_id)):
        data = fix_and_load_venv(storage, venv_id)
    if package.venv_config.freezefile_exposed():
        try:
            external_freeze = _load_external_freezefile(package)
        except CorruptedFreezefile:
            raise QuackPackError("Corrupted freeze file. Synchronize to regenerate.") from None
        if is_synchronized(package.manifest.summary, external_freeze, data, True):
            return external_freeze
        else:
            raise QuackPackError("Freeze file not synchronized with manifest. Synchronize to regenerate.")
    else:
        if data is not None:
            return data.freeze
        return None


def is_synchronized(
    summary: Summary | None,
    user_freeze: VenvFreeze | None,
    venv: StorageVenv | None,
    freezefile_exposed: bool,
) -> bool:
    if venv is None:
        logger.debug("Unsynchronized due to no StorageVenv")
        return False
    if (user_freeze is not None or freezefile_exposed) and user_freeze != venv.freeze:
        logger.debug("Unsynchronized due to freezes not matching")
        return False
    if summary is not None and not check_direct_dependencies(summary, venv.freeze):
        logger.debug("Unsynchronized due to direct dependencies not satisfied")
        return False
    return True
