import shutil
from pathlib import Path
from typing import override

from quackpack.core.solver.types.git_access import GitAccess
from quackpack.core.storage.paths import StoragePaths
from quackpack.core.types.manifest.source import GitSource
from quackpack.util.types.pkgid import GitPackageId


class StorageGitAccess(GitAccess):
    def __init__(self, paths: StoragePaths, cached: dict[GitSource, GitPackageId]):
        self._paths = paths
        self._cached = cached

    @override
    def git_path(self, url: str, commit: str) -> Path:
        return self._paths.pkg_dir(GitPackageId(url=url, commit=commit).upcast())

    @override
    def is_stored(self, url: str, commit: str) -> bool:
        return self._paths.package_stored(GitPackageId(url=url, commit=commit).upcast())

    @override
    def store(self, url: str, commit: str, source_path: Path) -> None:
        id = GitPackageId(url=url, commit=commit)
        dir = self._paths.pkg_dir(id.upcast())
        if dir.exists():
            shutil.rmtree(dir)
        dir.parent.mkdir(parents=True, exist_ok=True)
        source_path.rename(dir)

    @override
    def get_cached_git(
        self,
        url: str,
        commit: str | None = None,
        tag: str | None = None,
        branch: str | None = None,
    ) -> GitPackageId | None:
        return self._cached.get(
            GitSource(git_url=url, commit=commit, tag=tag, branch=branch)
        )
