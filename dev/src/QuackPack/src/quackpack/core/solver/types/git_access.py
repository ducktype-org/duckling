from abc import ABC, abstractmethod
from pathlib import Path

from quackpack.util.types.pkgid import GitPackageId


class GitAccess(ABC):
    @abstractmethod
    def git_path(self, url: str, commit: str) -> Path: ...

    @abstractmethod
    def is_stored(self, url: str, commit: str) -> bool: ...

    @abstractmethod
    def store(self, url: str, commit: str, source_path: Path) -> None: ...

    @abstractmethod
    def get_cached_git(
        self,
        url: str,
        commit: str | None = None,
        tag: str | None = None,
        branch: str | None = None,
    ) -> GitPackageId | None: ...
