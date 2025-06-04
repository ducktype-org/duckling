import asyncio
from abc import abstractmethod
from enum import Enum, auto
from pathlib import Path
from typing import override

from pydantic import AnyUrl

from quackpack.config.project import GitEntry, LocalEntry, Manifest, VersionList
from quackpack.fetcher import Fetcher
from quackpack.fetcher.api_types import MultiMetadata, SingleGitMetadataResult
from quackpack.project_loader import ProjectLoader
from quackpack.util.duckling_compatibility import is_valid_identifier
from quackpack.util.errors import QuackPackError
from quackpack.util.global_context import GlobalContext
from quackpack.util.lock import LockType
from quackpack.util.logger import get_logger
from quackpack.util.pkgid import Identifier
from quackpack.util.version import Version

logger = get_logger(__name__)


class FetchVersion(Enum):
    Tag = auto()


# FIXME(yaml): Bump to edytowalny manifest.
class NewDependencySource:
    # FIXME(yaml): W edytowalnym manifeście to pewnie będzie jakiś dict[str, str]?
    @abstractmethod
    def make_entry(self, ctx: GlobalContext) -> LocalEntry | GitEntry | VersionList: ...

    # FIXME: Move to async.
    @abstractmethod
    def get_manifest(self, ctx: GlobalContext) -> Manifest: ...


class GitSource(NewDependencySource):
    def __init__(self, url: str):
        self.url = url

    @override
    def make_entry(self, ctx: GlobalContext) -> GitEntry:
        _ = ctx
        logger.debug(f"making git entry for url {self.url}")
        return GitEntry(git_url=AnyUrl(self.url))

    @override
    def get_manifest(self, ctx: GlobalContext) -> Manifest:
        logger.debug(f"getting git manifest for url {self.url}")

        async def run_fetcher() -> SingleGitMetadataResult:
            with Fetcher(ctx) as fetcher:
                return await fetcher.clone_from_git(GitEntry(git_url=AnyUrl(self.url)))

        api_result = asyncio.run(run_fetcher())
        if api_result.result is not None:
            return api_result.result
        project = ProjectLoader.find_at_exact_directory(api_result.destination_path)
        return project.manifest_with_acquiring_lock(locktype=LockType.SHARED)


class LocalSource(NewDependencySource):
    def __init__(self, root: Path):
        if not root.is_dir():
            raise QuackPackError(f"local dependency root {root} does not point to the directory")
        self.root = root

    @override
    def make_entry(self, ctx: GlobalContext) -> LocalEntry:
        _ = ctx
        logger.debug(f"making local entry with root {self.root}")
        return LocalEntry(path=self.root)

    @override
    def get_manifest(self, ctx: GlobalContext) -> Manifest:
        _ = ctx
        logger.debug(f"getting local manifest with root {self.root}")
        dep = ProjectLoader.find_at_exact_directory(self.root)
        return dep.manifest_with_acquiring_lock(locktype=LockType.SHARED)


class RegistrySource(NewDependencySource):
    def __init__(self, name: str, version: str | FetchVersion):
        if not is_valid_identifier(name):
            raise QuackPackError(f"registry dependency {name} is not a valid identifier")
        self.name = Identifier(name)
        if isinstance(version, FetchVersion):
            self.version = version
            logger.debug("registry source will get manifest from server")
        else:
            self.version = Version.create_from_string(version)
            logger.debug(f"registry with specified version {self.version!s}")

    @override
    def get_manifest(self, ctx: GlobalContext) -> Manifest:
        _ = ctx
        # FIXME: Chyba gdy mamy podaną wersję, to chcemy sprawdzić, czy istnieje taka na serwerze.
        if isinstance(self.version, FetchVersion):
            return self.get_manifest_for_newest_package(ctx)
        return self.get_manifest_match(ctx, self.version)

    async def run_fetcher(self, ctx: GlobalContext) -> MultiMetadata | None:
        with Fetcher(ctx) as fetcher:
            res = await fetcher.get_package_all_metadata(AnyUrl(ctx.configuration.repository.url), self.name)
        return res.result

    def get_manifest_for_newest_package(self, ctx: GlobalContext) -> Manifest:
        metadata = asyncio.run(self.run_fetcher(ctx))
        if metadata is None:
            raise QuackPackError(f"No metadata for package {self.name!s}")
        metadata = metadata.packages_metadata
        if not metadata:
            raise QuackPackError(f"No metadata for package {self.name!s}")

        def sort_manifest_key(x: Manifest) -> Version:
            return x.metadata.version

        sorted(metadata, key=sort_manifest_key)
        return metadata[-1]

    def get_manifest_match(self, ctx: GlobalContext, version: Version) -> Manifest:
        metadata = asyncio.run(self.run_fetcher(ctx))

        if metadata is None:
            raise QuackPackError(f"No metadata for package {self.name!s}")

        metadata = metadata.packages_metadata
        if not metadata:
            raise QuackPackError(f"No metadata for package {self.name!s}")

        metadata = [x for x in metadata if version.can_be_upgraded_to(x.metadata.version)]
        if not metadata:
            raise QuackPackError(f"No matching versions of {version!s} for package {self.name!s}")

        def sort_manifest_key(x: Manifest) -> Version:
            return x.metadata.version

        sorted(metadata, key=sort_manifest_key)
        return metadata[-1]

    @override
    def make_entry(self, ctx: GlobalContext) -> VersionList:
        # NOTE: Bierzemy wersję z manifestu, żeby uniknąć takiej sytuacji:
        #       - podaliśmy dokładną wersję
        #       - fetcher znalazł wersję pasującą (jakoś)
        #       - ale feature flagi się rozjechały.
        version = self.get_manifest(ctx).metadata.version
        return VersionList([version])
