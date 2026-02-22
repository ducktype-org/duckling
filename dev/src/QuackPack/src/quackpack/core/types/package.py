from __future__ import annotations

from collections.abc import Iterator
from contextlib import contextmanager
from pathlib import Path

from tomlkit import TOMLDocument

from quackpack.core.signals import EnableInterrupt, RobustSignalHandler
from quackpack.core.types.manifest.manifest import Manifest
from quackpack.core.types.manifest.parse import parse_manifest
from quackpack.util.global_context import GlobalContext
from quackpack.util.lock import FileLock, LockType
from quackpack.util.logger import get_logger
from quackpack.util.toml_config import TOMLConfig
from quackpack.util.venv_config import VenvConfig

logger = get_logger(__name__)


class Package:
    def __init__(
        self,
        manifest_path: Path,
        venv_config_path: Path | None,
        local_storage_path: Path | None,
        ctx: GlobalContext,
    ) -> None:
        self._manifest_path = manifest_path.expanduser().resolve(strict=False)
        self._manifest = Package._read_manifest(self._manifest_path, ctx)

        if venv_config_path is not None:
            self._venv_config_path = venv_config_path.expanduser().resolve(strict=False)
        else:
            self._venv_config_path = None

        self._venv_config = Package._read_venv_config(self._venv_config_path)

        if local_storage_path is not None:
            self._local_storage_path = local_storage_path.expanduser().resolve(strict=False)
        else:
            self._local_storage_path = None

        self._is_global = False

    @staticmethod
    def global_venv(manifest_path: Path, ctx: GlobalContext) -> Package:
        package = Package(manifest_path, None, None, ctx)
        # Modify the venv_config of the package directly so that
        # chaning the venv_config file in the global package location
        # has no effect on the global venv config.
        venv_config = VenvConfig(config=TOMLConfig(content=TOMLDocument(), location=None))
        venv_config.set_freezefile_exposed(True)
        package._venv_config = venv_config
        package._is_global = True
        return package

    @contextmanager
    def lock(self, *, locktype: LockType = LockType.EXCLUSIVE, blocking: bool = True) -> Iterator[None]:
        lockfile = self.manifest_path.with_name(f".{self.manifest_path.stem}.lock")
        assert lockfile != self.manifest_path, "SoftwareFileLock can remove files"
        logger.debug(f"Locking {locktype!s} package using path `{lockfile}` with `blocking={blocking}`")
        with RobustSignalHandler():
            try:
                with FileLock(lockfile, locktype, blocking=blocking), EnableInterrupt():
                    yield
            finally:
                FileLock.try_delete_lock(lockfile)

    @property
    def manifest(self) -> Manifest:
        return self._manifest

    @property
    def venv_config(self) -> VenvConfig:
        return self._venv_config

    @classmethod
    def _read_manifest(cls, path: Path, ctx: GlobalContext) -> Manifest:
        """
        Helper method for deserializing manifest from `path`.
        """
        logger.debug(f"Reading package manifest from {path}")
        return parse_manifest(path, ctx)

    @classmethod
    def _read_venv_config(cls, path: Path | None) -> VenvConfig:
        """
        Helper method for deserializing venv config from `path`.
        """
        logger.debug(f"Reading package venv config from {path}")
        return VenvConfig.from_path_or_default(path)

    @property
    def manifest_path(self) -> Path:
        """
        Path to the package's manifest.
        """
        return self._manifest_path

    @property
    def venv_config_path(self) -> Path | None:
        """
        Path to the package's venv config.
        """
        return self._venv_config_path

    @property
    def local_storage_path(self) -> Path | None:
        """
        Path to the package's local storage.
        """
        return self._local_storage_path

    @property
    def package_root(self) -> Path:
        """
        Path to the package's root.
        """
        return self.manifest_path.parent

    def files_for_publish(self) -> list[Path]:
        result = [self.manifest_path]

        def add_dir(name: str | Path):
            path = self.package_root / name
            if path.is_dir():
                result.append(path)

        def add_file(name: str | Path):  # pyright: ignore[reportUnusedFunction]
            path = self.package_root / name
            if path.is_file():
                result.append(path)

        add_dir("src")
        return result

    @property
    def is_global(self) -> bool:
        return self._is_global
