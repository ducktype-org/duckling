from __future__ import annotations

from itertools import chain
from pathlib import Path
from typing import Final

from quackpack.project import Project
from quackpack.util.errors import QuackPackError
from quackpack.util.logger import get_logger

logger = get_logger(__name__)


class ProjectLoader:
    """
    This class' responsibility is to walk filesystem upwards looking for manifest,
    and returning `Project` object, when found, or raising an exception.
    """

    MANIFEST_NAME: Final[Path] = Path("quackconfig.yml")
    """
    Default manifest name.
    """

    @classmethod
    def find_from_directory(cls, start_path: Path) -> Project:
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
                logger.debug(f"Found Project at {path}")
                return Project(path)

        raise QuackPackError(f"No manifest found from {start_path} to {start_path.root}")

    @classmethod
    def find_from_file(cls, start_path: Path) -> Project:
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
        if start_path.name == ProjectLoader.MANIFEST_NAME.name:
            logger.debug(f"{start_path} is a manifest")
            return Project(start_path)
        # Note: We skip first parent, so we don't pick up manifest from same directory as `start_path`.
        return ProjectLoader.find_from_directory(start_path.parent.parent)

    @classmethod
    def find_at_exact_directory(cls, dir_path: Path) -> Project:
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
        return Project(destination)

    @classmethod
    def find_at_exact_file(cls, path: Path) -> Project:
        """
        Check, if `path` is a manifest.

        If `path` is not a file or has wrong name, exception is raised.
        """
        if path.is_file() and path.name == cls.MANIFEST_NAME.name:
            return Project(path)
        logger.debug(f"{path} is not a manifest")
        raise QuackPackError(f"{path} is not a manifest")

    @classmethod
    def find_from_cwd(cls) -> Project:
        """
        Convenient alias for `find_from_directory(Path.cwd())`.
        """
        return cls.find_from_directory(Path.cwd())
