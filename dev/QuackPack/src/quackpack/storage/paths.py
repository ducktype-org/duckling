from collections.abc import Generator
from pathlib import Path
from typing import Literal

from quackpack.util.pkgid import Identifier, ResolvedId

_LOCKS: Literal["locks"] = "locks"
_LOCKS_VENV_SYNC: Literal["venv_sync"] = "venv_sync"
_LOCKS_VENV_DATA: Literal["venv_data"] = "venv_data"
_CLEAN_LOCK_FILE: Literal["clean.lock"] = "clean.lock"

_VENV: Literal["venv"] = "venv"
_VENV_FILE: Literal["metadata"] = "metadata"
_VENV_BACKUP_FILE: Literal["metadata.old"] = _VENV_FILE + ".old"
_PKG: Literal["pkg"] = "pkg"


class StoragePaths:
    """
    Provides paths of the storage components, hiding the implementation details
    of the directory layout.
    """

    def __init__(self, storage: Path) -> None:
        self.storage = storage.expanduser().resolve()

    def pkg_dir(self, pkg_id: ResolvedId) -> Path:
        return self.storage / _PKG / pkg_id.dir_name()

    def venv_dir(self, venv_id: Identifier) -> Path:
        return self.storage / _VENV / str(venv_id)

    def venv_metadata(self, venv_id: Identifier) -> Path:
        return self.storage / _VENV / str(venv_id) / _VENV_FILE

    def venv_metadata_backup(self, venv_id: Identifier) -> Path:
        return self.storage / _VENV / str(venv_id) / _VENV_BACKUP_FILE

    def clean_lock(self) -> Path:
        return self.storage / _LOCKS / _CLEAN_LOCK_FILE

    def venv_sync_lock(self, venv_id: Identifier) -> Path:
        return self.storage / _LOCKS / _LOCKS_VENV_SYNC / str(venv_id)

    def venv_data_lock(self, venv_id: Identifier) -> Path:
        return self.storage / _LOCKS / _LOCKS_VENV_DATA / str(venv_id)

    def iter_pkgs(self) -> Generator[Path]:
        directory = self.storage / _PKG
        # TODO toctou
        return directory.iterdir() if directory.is_dir() else (_ for _ in ())

    def iter_venvs(self) -> Generator[Path]:
        directory = self.storage / _VENV
        # TODO toctou
        return directory.iterdir() if directory.is_dir() else (_ for _ in ())

    def iter_sync_locks(self) -> Generator[Path]:
        directory = self.storage / _LOCKS / _LOCKS_VENV_SYNC
        # TODO toctou
        return directory.iterdir() if directory.is_dir() else (_ for _ in ())

    def iter_data_locks(self) -> Generator[Path]:
        directory = self.storage / _LOCKS / _LOCKS_VENV_DATA
        # TODO toctou
        return directory.iterdir() if directory.is_dir() else (_ for _ in ())
