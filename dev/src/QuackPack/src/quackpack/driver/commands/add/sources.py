import asyncio
from abc import abstractmethod
from enum import Enum, auto
from pathlib import Path
from typing import override

from quackpack.core.fetcher.api_types import MultiMetadata, SingleGitMetadataResult, SingleMetadata
from quackpack.core.fetcher.fetcher import FetcherContext
from quackpack.core.package_loader import PackageLoader
from quackpack.core.types.manifest.schemas.manifest import DependencySchema, OredSemverSchema, SourceSchema
from quackpack.core.types.manifest.source import GitSource as CoreGitSource
from quackpack.core.types.manifest.summary import Summary
from quackpack.core.types.package import Package
from quackpack.util.duckling_compatibility import is_valid_identifier
from quackpack.util.global_context import GlobalContext
from quackpack.util.logger import get_logger
from quackpack.util.types.errors import QuackPackError
from quackpack.util.types.pkgid import Identifier
from quackpack.util.types.version import Version

logger = get_logger(__name__)


class FetchVersion(Enum):
    Tag = auto()


class NewDependencySource:
    @abstractmethod
    def make_entry(self, ctx: GlobalContext, root: Package) -> DependencySchema: ...

    @abstractmethod
    def get_summary(self, ctx: GlobalContext) -> Summary: ...


class GitSource(NewDependencySource):
    def __init__(self, url: str):
        self.url = url

    @override
    def make_entry(self, ctx: GlobalContext, root: Package) -> DependencySchema:
        _ = ctx
        _ = root
        logger.debug(f"making git entry for url `{self.url}`")
        return DependencySchema(source=SourceSchema.of_git(self.url))

    @override
    def get_summary(self, ctx: GlobalContext) -> Summary:
        logger.debug(f"getting git manifest for url `{self.url}`")

        async def run_fetcher() -> SingleGitMetadataResult:
            with FetcherContext(ctx) as fetcher:
                return await fetcher.clone_from_git(CoreGitSource(git_url=self.url))

        api_result = asyncio.run(run_fetcher())
        if api_result.result is not None:
            return Summary.from_schema(api_result.result)
        package = PackageLoader.find_at_exact_directory(api_result.destination_path, ctx)
        return package.manifest.summary


class LocalSource(NewDependencySource):
    def __init__(self, root: Path):
        if not root.is_dir():
            raise QuackPackError(f"local dependency root `{root!s}` does not point to the directory")
        self.root = root

    @override
    def make_entry(self, ctx: GlobalContext, root: Package) -> DependencySchema:
        _ = ctx
        if self.root.expanduser().is_absolute():
            to_insert = self.root
        else:
            to_insert = self.root.resolve().relative_to(root.package_root, walk_up=True)
        logger.debug(f"making local entry with root `{to_insert}`")
        return DependencySchema(source=SourceSchema.of_path(to_insert))

    @override
    def get_summary(self, ctx: GlobalContext) -> Summary:
        _ = ctx
        logger.debug(f"getting local manifest with root `{self.root}`")
        dep = PackageLoader.find_at_exact_directory(self.root, ctx)
        return dep.manifest.summary


class RegistrySource(NewDependencySource):
    def __init__(self, name: str, version: str | FetchVersion):
        if not is_valid_identifier(name):
            raise QuackPackError(f"registry dependency `{name}` is not a valid identifier")
        self.name = Identifier(name)
        if isinstance(version, FetchVersion):
            self.version = version
            logger.debug("registry source will get manifest from server")
        else:
            self.version = Version.create_from_string(version)
            logger.debug(f"registry with specified version `{self.version!s}`")

    @override
    def get_summary(self, ctx: GlobalContext) -> Summary:
        _ = ctx
        if isinstance(self.version, FetchVersion):
            return self.get_summary_for_newest_package(ctx)
        return self.get_summary_match(ctx, self.version)

    async def run_fetcher(self, ctx: GlobalContext) -> MultiMetadata | None:
        with FetcherContext(ctx) as fetcher:
            res = await fetcher.get_package_all_metadata(ctx.registry_url(), self.name)
        return res.result

    def get_summary_for_newest_package(self, ctx: GlobalContext) -> Summary:
        metadata = asyncio.run(self.run_fetcher(ctx))

        if metadata is None:
            raise QuackPackError(f"registry has no metadata for package `{self.name!s}`")

        metadata = metadata.packages_metadata
        if not metadata:
            logger.debug(metadata)
            raise QuackPackError(
                f"registry has some metadata for package `{self.name!s}`, but returned an empty list"
            )

        def sort_manifest_key(x: SingleMetadata) -> Version:
            return Version.create_from_string(x.metadata.version.root)

        sorted(metadata, key=sort_manifest_key)
        return Summary.from_schema(metadata[-1])

    def get_summary_match(self, ctx: GlobalContext, version: Version) -> Summary:
        metadata = asyncio.run(self.run_fetcher(ctx))

        if metadata is None:
            raise QuackPackError(f"registry has no metadata for package `{self.name!s}`")

        metadata = metadata.packages_metadata
        if not metadata:
            logger.debug(metadata)
            raise QuackPackError(
                f"registry has some metadata for package `{self.name!s}`, but returned an empty list"
            )

        metadata = [
            x
            for x in metadata
            if version.can_be_upgraded_to(Version.create_from_string(x.metadata.version.root))
        ]
        if not metadata:
            raise QuackPackError(f"No matching versions of `{version!s}` for package `{self.name!s}`")

        def sort_manifest_key(x: SingleMetadata) -> Version:
            return Version.create_from_string(x.metadata.version.root)

        sorted(metadata, key=sort_manifest_key)
        return Summary.from_schema(metadata[-1])

    @override
    def make_entry(self, ctx: GlobalContext, root: Package) -> DependencySchema:
        _ = root
        # NOTE: Bierzemy wersję z manifestu, żeby uniknąć takiej sytuacji:
        #       - podaliśmy dokładną wersję
        #       - fetcher znalazł wersję pasującą (jakoś)
        #       - ale feature flagi się rozjechały.
        version = self.get_summary(ctx).version
        schema = OredSemverSchema(str(version))
        return DependencySchema(version=schema)
