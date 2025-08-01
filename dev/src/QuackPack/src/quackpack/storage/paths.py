from collections.abc import Generator
from pathlib import Path
from typing import Literal

from quackpack.util.logger import get_logger
from quackpack.util.types.pkgid import Identifier, PackageId

logger = get_logger(__name__)


_LOCKS: Literal["locks"] = "locks"
_LOCKS_VENV_SYNC: Literal["venv_sync"] = "venv_sync"
_LOCKS_VENV_DATA: Literal["venv_data"] = "venv_data"
_CLEAN_LOCK_FILE: Literal["clean.lock"] = "clean.lock"

_VENV: Literal["venv"] = "venv"
_VENV_FILE: Literal["metadata"] = "metadata"
_VENV_BACKUP_FILE: Literal["metadata.old"] = _VENV_FILE + ".old"
_PKG: Literal["pkg"] = "pkg"

_CHECKSUM_FILE: Literal["checksum.txt"] = "checksum.txt"


class StoragePaths:
    """
    Provides paths of the storage components, hiding the implementation details
    of the directory layout.
    """

    def __init__(self, storage: Path) -> None:
        self.storage = storage.expanduser().resolve()
        logger.debug(f"Created StoragePaths with storage path {self.storage!s}")

    def pkg_dir(self, pkg_id: PackageId) -> Path:
        return self.storage / _PKG / pkg_id.storage_name()

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
        """
        Returns iterator over all packages in the storage.
        The yielded packages need not be correct (may be missing checksum).
        It is not guaranteed that during iteration, the yielded paths
        still exist and there are no guarantees on paths that appeared during iteration.
        """
        directory = self.storage / _PKG
        return directory.iterdir() if directory.is_dir() else (_ for _ in ())

    def iter_venvs(self) -> Generator[Path]:
        """
        Returns iterator over all packages in the storage.
        The yielded venvs need not be correct (may have invalid data).
        It is not guaranteed that during iteration, the yielded paths
        still exist and there are no guarantees on paths that appeared during iteration.
        """
        directory = self.storage / _VENV
        return directory.iterdir() if directory.is_dir() else (_ for _ in ())

    def iter_sync_locks(self) -> Generator[Path]:
        """
        Returns iterator over all sync locks in the storage.
        It is not guaranteed that during iteration, the yielded paths
        still exist and there are no guarantees on paths that appeared during iteration.
        """
        directory = self.storage / _LOCKS / _LOCKS_VENV_SYNC
        return directory.iterdir() if directory.is_dir() else (_ for _ in ())

    def iter_data_locks(self) -> Generator[Path]:
        """
        Returns iterator over all data locks in the storage.
        It is not guaranteed that during iteration, the yielded paths
        still exist and there are no guarantees on paths that appeared during iteration.
        """
        directory = self.storage / _LOCKS / _LOCKS_VENV_DATA
        return directory.iterdir() if directory.is_dir() else (_ for _ in ())

    def package_stored(self, id: PackageId) -> bool:
        if id.is_local():
            return False
        dir = self.pkg_dir(id)
        return dir.is_dir() and (dir / _CHECKSUM_FILE).exists()

    def add_checksum(self, id: PackageId) -> None:
        assert not id.is_local()
        dir = self.pkg_dir(id)
        (dir / _CHECKSUM_FILE).touch()
