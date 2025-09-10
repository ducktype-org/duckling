"""
Provides operations on the virtual environment state file, which preserve
coherency of the storage. Also exposes simpler helpers for general
system-failure safe file operations, which are be used to build
up higher level operations.



The state of virtual environment is changed with following invariants:

1. Each existring venv holds at least one of `metadata` or `metadata.old` files,
   each one of them internally holds a checksum that assures its validity.
   File that exists on disk and contains a correct checksum is considered valid.
2. At any point (also during any of the lower level operations), for any
   existing venv, at least one of the files is valid. If both of them are,
   the version held in ``metadata`` is considered to be the current state.
   Venv with both files unvalid is considered nonexistent.
3. We never change the state of the venv from existing to nonexistent (with
   operations provided in this module, other methods like removing venv
   directory may cause such change). Also, after new state has been saved,
   and becomes valid we do not regress to considering older state to be current.
4. A state in which venv exists and ``metadata`` holds current state, or venv does
   not exist and the venv directory is empty or does not exist will be named "canonical".

There are two operations:

- fix_and_load: transforms state of venv into canonical form. Loads the state
  in the process (we do not provide seperate fix and load, as checking validity
  of state requires reading the state, so we just return that result)
- save: requires that the state is in canonical form, transforms the state
  into a new canonical form with a new current state set to the provided.

The format of the virtual environment file is:

- JSON_DATA representing StorageVenv object,
- in a new line a sha256 hash of the preceding data for content validity checking.

Some considerations:

It may seem that state coherency can be achieved in a simpler way, by creating
a new one, and moving some files around. Due to insufficient guarantees
from some operating systems, we try to minimize creating, moving and deleting files:
`os.fsync` for files is supported on most platforms, while `os.fsync`
for directories does not work for example on Windows, which makes it hard
to guarantee, which files will exist and where after system failure.
Instead we build higher level operations using ``transfer_file`` function,
which copies contents of a file and executes ``fsync`` on it.

Note that when creating a new virtual environment, operating system might
break some invariants, by not flushing directory entries to the disk.
If directory ``fsync`` is supported, we use that, but in general, if system
failure happens during that window, we cannot guarantee the virtual environment
to exist after reboot even if it has been used before. The problem affects
however only relatively new virtual environments.
"""

from __future__ import annotations

import os
import shutil
from hashlib import sha256
from pathlib import Path
from typing import Any, Final, cast

from pydantic import BaseModel, field_serializer, field_validator
from pydantic_core import ValidationError

from quackpack.core.signals import EnableInterrupt
from quackpack.core.storage.paths import StoragePaths
from quackpack.core.types.manifest.schemas.registry import GitSourceSchema, RegistryManifestSchema
from quackpack.core.types.manifest.source import GitSource
from quackpack.util.context_managers import OsFdContext
from quackpack.util.types.pkgid import GitPackageId, Identifier, PackageId


def transfer_file(*, source: Path, target: Path) -> None:
    """
    Copy the contents of one file into another and synchronize the result to disk.

    :param pathlib.Path source: Source file to copy from.
    :param pathlib.Path target: Destination file to copy into.
    """

    BUFFER_SIZE: Final[int] = 4096
    with open(source, "rb") as src, open(target, "wb") as dst:
        while chunk := src.read(BUFFER_SIZE):
            dst.write(chunk)
        dst.flush()
        os.fsync(dst.fileno())


def try_fsync_dir(dir: Path) -> None:
    """
    Ensure that directory-level changes (creation, deletion, renaming) are persisted to disk.

    This is a best-effort operation. If unsupported, it will be silently skipped.

    :param pathlib.Path dir: Directory to synchronize.
    """

    try:
        with OsFdContext(dir, os.O_RDONLY) as dir_fd:
            os.fsync(dir_fd)
    except (NotImplementedError, OSError):
        pass


class _CorruptedFileError:
    pass


class PackageFreeze(BaseModel):
    """
    Information required to build a package in a given dependencies realization.

    :ivar dict[quackpack.util.pkgid.Identifier, quackpack.util.pkgid.PackageId] dependencies: Dependencies of a given package.
    :ivar list[quackpack.util.pkgid.Identifier] used_flags: Flags used to compile the dependency.
    :ivar list[str] | None system: List of required operating systems.
    :ivar list[str] | None arch: List of required CPU architectures.
    """

    dependencies: dict[Identifier, PackageId]
    used_flags: list[Identifier]
    system: list[str] | None = None
    arch: list[str] | None = None


class _Dependency(BaseModel):
    id: PackageId
    data: PackageFreeze


class _GitFetchCacheEntry(BaseModel):
    source: GitSource
    result: GitPackageId

    @field_validator("source", mode="before")
    @classmethod
    def deserialize_source(cls, value: Any) -> GitSource:
        if isinstance(value, GitSource):
            return value
        value = GitSourceSchema.model_validate(value)
        # this should be `GitSource.from_schema`, but for dumb python reason it cannot exist
        return GitSource(git_url=value.git_url, commit=value.commit, tag=value.tag, branch=value.branch)

    @field_serializer("source")
    def serialize_source(self, source: GitSource) -> GitSourceSchema:
        return cast(GitSourceSchema, source.into_schema().inner)


class VenvFreeze(BaseModel):
    """
    Realization of requirements stored in virtual environment manifest.
    Contains all the information required to build and run code using the given virtual environment.

    :ivar dict[quackpack.util.pkgid.Identifier, quackpack.util.pkgid.PackageId] direct_dependencies:
        Mapping from direct dependency identifiers to concrete packages.
    :ivar dict[quackpack.util.pkgid.PackageId, quackpack.storage.files.PackageFreeze] dependencies:
        Build graph of the realization, together with feature flags used for each package.
    :ivar dict[quackpack.manifest.source.GitSource, quackpack.util.pkgid.GitPackageId] git_fetch_cache:
        Cache of associations from unresolved git sources (which may use branch or tag) to conrete git ids.
        Updated on each freeze file creation to store all the git sources that appreared in the solving process.
        The values of the cache are required to exist as keys to `dependencies`.
    """

    direct_dependencies: dict[Identifier, PackageId]
    dependencies: dict[PackageId, PackageFreeze]
    git_fetch_cache: dict[GitSource, GitPackageId]

    @field_validator("dependencies", mode="before")
    @classmethod
    def deserialize_dependencies(cls, value: Any) -> dict[PackageId, PackageFreeze]:
        if isinstance(value, dict):
            entries = (
                _Dependency.model_validate({"id": k, "data": v})
                for k, v in cast(dict[Any, Any], value).items()
            )
        elif isinstance(value, list):
            entries = (_Dependency.model_validate(dep) for dep in cast(list[Any], value))
        else:
            raise ValueError("Expected dict or list of dependency entries")
        return {dep.id: dep.data for dep in entries}

    @field_serializer("dependencies")
    def serialize_dependencies(self, dependencies: dict[PackageId, PackageFreeze]) -> list[_Dependency]:
        return [_Dependency(id=k, data=v) for k, v in dependencies.items()]

    @field_validator("git_fetch_cache", mode="before")
    @classmethod
    def deserialize_git_fetch_cache(cls, value: Any) -> dict[GitSource, GitPackageId]:
        if isinstance(value, dict):
            entries = (
                _GitFetchCacheEntry.model_validate({"source": k, "result": v})
                for k, v in cast(dict[Any, Any], value).items()
            )
        elif isinstance(value, list):
            entries = (_GitFetchCacheEntry.model_validate(entry) for entry in cast(list[Any], value))
        else:
            raise ValueError("Expected dict or list of git fetch cache entries")
        return {entry.source: entry.result for entry in entries}

    @field_serializer("git_fetch_cache")
    def serialize_git_fetch_cache(
        self, git_fetch_cache: dict[GitSource, GitPackageId]
    ) -> list[_GitFetchCacheEntry]:
        return [
            _GitFetchCacheEntry(source=source, result=result) for source, result in git_fetch_cache.items()
        ]


class StorageVenv(BaseModel):
    """
    State of virtual environment in the storage. Stores the freeze for the given
    virtual environment, copy of manifest's metadata, and additional info
    required for storage functioning: last location and access info.

    :ivar quackpack.storage.files.VenvFreeze freeze: Realization of dependency requirements.
    :ivar quackpack.schemas.manifest.ManifestSchema original_schema: Copy of the corresponding manifest.
    :ivar pathlib.Path last_location: Filesystem path of the last environment location.
    :ivar float last_modification: Timestamp of the last modification.
    :ivar float last_access: Timestamp of the last access.
    """

    freeze: VenvFreeze
    original_schema: RegistryManifestSchema
    is_ephemeral: bool
    last_location: Path
    last_modification: float
    last_access: float

    @staticmethod
    def load(path: Path) -> StorageVenv | _CorruptedFileError:
        """
        Load and validate a virtual environment state file.

        :param pathlib.Path path: Path to the metadata file.
        :return: Parsed :class:`StorageVenv` object if valid, :class:`_CorruptedFileError` otherwise.
        :rtype: quackpack.storage.files.StorageVenv | quackpack.storage.venv._CorruptedFileError
        """

        with EnableInterrupt(), open(path, encoding="utf-8") as f:
            content = f.read()
        try:
            data, checksum = content.rsplit("\n", maxsplit=1)
        except ValueError:
            return _CorruptedFileError()

        hash = sha256()
        hash.update(bytes(data, encoding="utf-8"))
        if checksum != hash.hexdigest():
            return _CorruptedFileError()

        try:
            return StorageVenv.model_validate_json(data)
        except ValidationError:
            return _CorruptedFileError()

    def save(self, path: Path) -> None:
        """
        Write a virtual environment state to disk with content validation checksum.

        :param pathlib.Path path: Path to save the metadata file to.
        :param quackpack.storage.files.StorageVenv venv: The environment data to store.
        """

        data = self.model_dump_json()
        with open(path, "w", encoding="utf-8") as f:
            hash = sha256()
            hash.update(bytes(data, encoding="utf-8"))
            with EnableInterrupt():
                f.write(f"{data}\n{hash.hexdigest()}")
                f.flush()
                os.fsync(f.fileno())


def fix_and_load_venv(storage: StoragePaths, venv_id: Identifier) -> StorageVenv | None:
    """
    Convert the state of a virtual environment into canonical form and return its state.

    If neither the main nor backup file is valid, the environment directory is removed.

    :param quackpack.storage.paths.StoragePaths storage: Storage layout manager.
    :param quackpack.util.pkgid.Identifier venv_id: Identifier of the virtual environment.
    :return: The loaded and validated virtual environment state, or None if it does not exist.
    :rtype: quackpack.storage.venv.StorageVenv | None
    """

    # NOTE: when external entity changes the storage disregarding the rules, we have
    # toctou here and an exception might be thrown later. We ignore that to keep sanity.
    if not storage.venv_dir(venv_id).is_dir():
        return None
    path = storage.venv_metadata(venv_id)
    backup_path = storage.venv_metadata_backup(venv_id)
    existed = path.exists()
    backup_existed = backup_path.exists()
    # if main file is valid, return state held in it
    if existed:
        data = StorageVenv.load(path)
        if not isinstance(data, _CorruptedFileError):
            return data
    # otherwise, the state is not canonical, and current state, if it exists,
    # is held in the backup file
    if backup_existed:
        data = StorageVenv.load(backup_path)
        if not isinstance(data, _CorruptedFileError):
            # transform the state into canonical one, by moving the valid
            # backup into main file
            with EnableInterrupt():
                transfer_file(source=backup_path, target=path)
            if not existed:
                with EnableInterrupt():
                    try_fsync_dir(storage.venv_dir(venv_id))
            return data
    # both files are not valid, so the venv does not exist,
    # put it in the canonical form by deleting its directory
    with EnableInterrupt():
        shutil.rmtree(storage.venv_dir(venv_id))
    return None


def save_venv(storage: StoragePaths, venv_id: Identifier, venv_data: StorageVenv) -> None:
    """
    Save a new canonical state of the virtual environment to storage.

    Assumes that the current ``metadata`` file is valid. This is typically ensured
    by calling :func:`fix_and_load_venv` before.

    :param quackpack.storage.paths.StoragePaths storage: Storage layout manager.
    :param quackpack.util.pkgid.Identifier venv_id: Identifier of the virtual environment.
    :param quackpack.storage.files.StorageVenv venv_data: New environment state to persist.
    """

    path = storage.venv_metadata(venv_id)
    backup_path = storage.venv_metadata_backup(venv_id)
    existed = path.exists()
    backup_existed = backup_path.exists()
    if not path.parent.exists():
        with EnableInterrupt():
            path.parent.mkdir(parents=True, exist_ok=True)
    if existed:
        # move old current state to backup file, as when error occurs during
        # overwriting the main file, the invariants will be upkept.
        # (the backup file will be valid)
        with EnableInterrupt():
            transfer_file(source=path, target=backup_path)
        if not backup_existed:
            with EnableInterrupt():
                try_fsync_dir(storage.venv_dir(venv_id))
    venv_data.save(path)
    if not existed:
        # also initialize the `.old` file, such that issues
        # relating to unavailable directory `fsync` are minimized
        with EnableInterrupt():
            transfer_file(source=path, target=backup_path)
            try_fsync_dir(storage.venv_dir(venv_id))
