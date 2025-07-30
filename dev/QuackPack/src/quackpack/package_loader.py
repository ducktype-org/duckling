from __future__ import annotations

from contextlib import suppress
from itertools import chain
from pathlib import Path
from typing import Final

from quackpack.global_context import GlobalContext
from quackpack.manifest.editable import EditableManifest
from quackpack.manifest.schemas.manifest import ManifestSchema, MetadataSchema, SemverSchema
from quackpack.package import Package
from quackpack.util.logger import get_logger
from quackpack.util.types.errors import QuackPackError
from quackpack.util.types.version import Version

logger = get_logger(__name__)


class PackageLoader:
    """
    This class' responsibility is to walk filesystem upwards looking for manifest,
    and returning `Package` object, when found, or raising an exception.
    """

    MANIFEST_NAME: Final[Path] = Path("quackconfig.yml")
    """
    Default manifest name.
    """

    FREEZE_NAME: Final[Path] = Path("quackfreeze.json")
    """
    Default manifest name.
    """

    VENV_CONFIG_NAME: Final[Path] = Path("venvconfig.toml")
    """
    Default virtual environment name.
    """

    LOCAL_STORAGE_NAME: Final[Path] = Path(".storage")
    """
    Default virtual environment name.
    """

    @classmethod
    def venv_config_path(cls, path: Path) -> Path | None:
        potential_path = path / cls.VENV_CONFIG_NAME
        return potential_path if potential_path.is_file() else None

    @classmethod
    def local_storage_path(cls, path: Path) -> Path | None:
        potential_path = path / cls.LOCAL_STORAGE_NAME
        return potential_path if potential_path.is_dir() else None

    @classmethod
    def find_from_directory(cls, start_path: Path, ctx: GlobalContext, allow_global: bool = False) -> Package:
        """
        Start searching from directory `start_path`.

        If `start_path` is not a directory or no manifest was found from `start_path` up to root,
        exception is raised.
        """
        start_path = start_path.expanduser()
        if not start_path.is_dir():
            logger.debug(f"{start_path} is not a dir")
            raise NotADirectoryError(start_path)
        logger.debug(f"Starting to look for manifest from {start_path}")

        start_path = start_path.resolve(strict=True)
        for potential_location in chain((start_path,), start_path.parents):
            path = potential_location / cls.MANIFEST_NAME
            if path.is_file():
                logger.debug(f"Found package at {path}")
                venv_config_location = cls.venv_config_path(potential_location)
                if venv_config_location:
                    logger.debug(f"Found venv config for package at {path}")

                local_storage_path = cls.local_storage_path(potential_location)
                return Package(path, venv_config_location, local_storage_path, ctx)

        if allow_global:
            return cls.global_package(ctx)
        else:
            raise QuackPackError(f"No manifest found from `{start_path}` to `{start_path.root}`")

    @classmethod
    def find_from_file(cls, start_path: Path, ctx: GlobalContext, allow_global: bool = False) -> Package:
        """
        Firstly check, if `start_path` is manifest, otherwise search from `start_path`'s parent parent.
        (As you'd `cd ..`, if you are in directory with `start_path`).

        If `start_path` is not a file or no manifest was found from `start_path` up to root,
        exception is raised.
        """
        start_path = start_path.expanduser()
        if not start_path.is_file():
            logger.debug(f"{start_path} is not a file")
            raise FileNotFoundError(start_path)
        if start_path.name == PackageLoader.MANIFEST_NAME.name:
            logger.debug(f"{start_path} is a manifest")
            venv_config_location = cls.venv_config_path(start_path.parent)
            if venv_config_location:
                logger.debug(f"Found venv config for package at {start_path.parent}")

            local_storage_path = cls.local_storage_path(start_path.parent)
            return Package(start_path, venv_config_location, local_storage_path, ctx)
        # Note: We skip first parent, so we don't pick up manifest from same directory as `start_path`.
        return PackageLoader.find_from_directory(start_path.parent.parent, ctx, allow_global=allow_global)

    @classmethod
    def find_at_exact_directory(cls, dir_path: Path, ctx: GlobalContext) -> Package:
        """
        Look for manifest in `dir_path`, and don't traverse filesystem.

        If `dir_path` is not a directory or it doesn't contain manifest, exception is raised.
        """
        if not dir_path.is_dir():
            logger.debug(f"{dir_path} is not a dir")
            raise NotADirectoryError(dir_path)
        destination = dir_path / cls.MANIFEST_NAME
        if not destination.is_file():
            logger.debug(f"{dir_path} has no manifest")
            raise QuackPackError(f"There is no manifest at {dir_path}")

        venv_config_location = cls.venv_config_path(dir_path)
        if venv_config_location:
            logger.debug(f"Found venv config for package at {dir_path}")

        local_storage_path = cls.local_storage_path(dir_path)
        return Package(destination, venv_config_location, local_storage_path, ctx)

    @classmethod
    def find_at_exact_file(cls, path: Path, ctx: GlobalContext) -> Package:
        """
        Check, if `path` is a manifest.

        If `path` is not a file or has wrong name, exception is raised.
        """
        if path.is_file() and path.name == cls.MANIFEST_NAME.name:
            venv_config_location = cls.venv_config_path(path.parent)
            if venv_config_location:
                logger.debug(f"Found venv config for package at {path.parent}")

            local_storage_path = cls.local_storage_path(path.parent)
            return Package(path, venv_config_location, local_storage_path, ctx)
        logger.debug(f"{path} is not a manifest")
        raise QuackPackError(f"{path} is not a manifest")

    @classmethod
    def find_from_cwd(cls, ctx: GlobalContext, allow_global: bool = False) -> Package:
        """
        Convenient alias for `find_from_directory(Path.cwd())`.
        """
        return cls.find_from_directory(Path.cwd(), ctx, allow_global=allow_global)

    @classmethod
    def global_package(cls, ctx: GlobalContext) -> Package:
        dir = ctx.ensure_global_venv_dir()
        manifest_path = dir / cls.MANIFEST_NAME
        # This check is not really needed as we have "x" flag later paired with suppress,
        # but will in most cases help avoid the operations inside.
        if not manifest_path.exists():
            metadata = MetadataSchema(version=SemverSchema(str(Version.default())), name="global")
            manifest = ManifestSchema(metadata=metadata)
            saveable_manifest = EditableManifest.create_with_data(
                manifest.model_dump(exclude_none=True, exclude_unset=True)
            )
            with suppress(FileExistsError), open(manifest_path, mode="x") as f:
                f.write(saveable_manifest.as_str())
        return Package.global_venv(manifest_path, ctx)
