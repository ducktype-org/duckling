from collections.abc import Iterator
from contextlib import contextmanager, nullcontext
from pathlib import Path

from quackpack.config.project import Manifest
from quackpack.signals import EnableInterrupt, RobustSignalHandler
from quackpack.util.deser.deserialize import deserialize
from quackpack.util.deser.errors import ConfigFileLoadError
from quackpack.util.errors import QuackPackError
from quackpack.util.lock import FileLock, LockType
from quackpack.util.logger import get_logger

logger = get_logger(__name__)


class Project:
    def __init__(self, manifest_path: Path) -> None:
        self._manifest_path = manifest_path.expanduser().resolve(strict=False)
        self._manifest: Manifest | None = None

    @contextmanager
    def lock(
        self,
        *,
        locktype: LockType = LockType.EXCLUSIVE,
        blocking: bool = True,
        enable_robust_handler: bool = True,
    ) -> Iterator[None]:
        lockfile = self.manifest_path.with_name(f".{self.manifest_path.stem}.lock")
        assert lockfile != self.manifest_path, "SoftwareFileLock can remove files"
        logger.debug(
            f"Locking {locktype!s} project using path '{lockfile}' with signals={enable_robust_handler} and blocking={blocking}"
        )
        with RobustSignalHandler() if enable_robust_handler else nullcontext():
            try:
                with FileLock(lockfile, locktype, blocking=blocking), EnableInterrupt():
                    yield
            finally:
                FileLock.try_delete_lock(lockfile)

    def manifest_without_acquiring_lock(self) -> Manifest:
        """
        Load manifest without acquiring manifest's lock.
        """
        logger.debug("Reloading Venv manifest without locking")
        if self._manifest is None:
            self._manifest = Project._read_manifest(self.manifest_path)
        return self._manifest

    def manifest_with_acquiring_lock(
        self,
        *,
        locktype: LockType = LockType.EXCLUSIVE,
        blocking: bool = True,
        enable_robust_handler: bool = True,
    ) -> Manifest:
        """
        Load manifest with acquiring manifest's lock.
        """
        logger.debug("Reloading project manifest with locking")
        if self._manifest is None:
            with self.lock(locktype=locktype, blocking=blocking, enable_robust_handler=enable_robust_handler):
                self._manifest = Project._read_manifest(self.manifest_path)
        return self._manifest

    def save_to_disk(self) -> None:
        """
        Save any changes made to the project into disk.
        """
        logger.debug(f"Saving Project configuration to {self.manifest_path}")
        # FIXME: Z lockiem/bez locka, czy jakiś check na self._manifest == None?
        self.manifest_without_acquiring_lock().save_to(self.manifest_path)

    @classmethod
    def _read_manifest(cls, path: Path) -> Manifest:
        """
        Helper method for deserializing manifest from `path`.
        """
        logger.debug(f"Reading project manifest from {path}")
        try:
            return deserialize(path, Manifest)
        except ConfigFileLoadError as e:
            raise QuackPackError(e) from e

    @property
    def manifest_path(self) -> Path:
        """
        Path to the project's manifest.
        """
        return self._manifest_path

    @property
    def project_root(self) -> Path:
        """
        Path to the project's root.
        """
        return self.manifest_path.parent

    def files_for_publish(self) -> list[Path]:
        result = [self.manifest_path]

        def add_dir(name: str | Path):
            path = self.project_root / name
            if path.is_dir():
                result.append(path)

        def add_file(name: str | Path):
            path = self.project_root / name
            if path.is_file():
                result.append(path)

        add_dir("src")
        return result
