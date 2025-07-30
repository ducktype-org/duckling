"""
Module for fetching package metadata and blobs from Ducknest instances and cloning Git repositories.
----
Classes:
- `Fetcher`: A class for managing HTTP and Git clients, and caching metadata.
- `FetcherContext`: Context manager for safely initializing a Fetcher with required dependencies.
"""

from contextlib import AbstractContextManager, ExitStack, suppress
from dataclasses import dataclass
from pathlib import Path
from tempfile import mkdtemp, mktemp
from types import TracebackType
from typing import Final, override

from rich import console, progress

from quackpack.fetcher.api_types import (
    MultiMetadataResult,
    Package as FetcherPackage,
    PackageName,
    SearchResult,
    SingleGitMetadataResult,
    SingleMetadataResult,
    URLType,
)
from quackpack.fetcher.cache import MetadataCache, MetadataCacheContext
from quackpack.fetcher.client import DucknestClient, DucknestClientContext, GitClient
from quackpack.fetcher.util.compression import pack_to_file
from quackpack.fetcher.util.curl_progress import CurlProgress
from quackpack.fetcher.util.git_progress import GitRemoteProgress
from quackpack.global_context import GlobalContext
from quackpack.manifest.source import GitSource
from quackpack.package import Package
from quackpack.util.logger import get_logger
from quackpack.util.types.errors import QuackPackError

logger = get_logger(__name__)


@dataclass(frozen=True, kw_only=True)
class Progresses:
    blobs: progress.Progress
    total: progress.Progress
    bundled: console.Group
    git: progress.Progress


class Fetcher:
    """
    A class for managing HTTP and Git clients, and caching metadata.

    :param ctx: The global context containing configuration for cache paths.
    :type ctx: quackpack.util.global_context.GlobalContext
    """

    DEFAULT_BLOB_FILENAME: Final[str] = "source.tar.gz"

    def __init__(
        self,
        ctx: GlobalContext,
        ducknest_client: DucknestClient,
        cache: MetadataCache,
        git_client: GitClient,
        progresses: Progresses,
    ):
        self.metadata_cache_db_path = ctx.ensure_metadata_db()
        self.download_cache_path = ctx.ensure_download_dir()
        self.artifacts_cache_path = ctx.ensure_artifacts_dir()

        logger.debug(
            f"Initializing fetcher with {self.metadata_cache_db_path=} and {self.download_cache_path=}"
        )

        self.ducknest_client = ducknest_client
        self.cache = cache
        self.git_client = git_client

        self.blob_progress = progresses.blobs
        self.total_progress = progresses.total
        self.git_progress = progresses.git
        self.bundled_progress = progresses.bundled

        self.curl_client_progress = CurlProgress(self.blob_progress, self.total_progress)

        self.ctx = ctx

    async def get_package_metadata(
        self, instance_url: URLType, package: FetcherPackage
    ) -> SingleMetadataResult:
        """
        Retrieve metadata for a specific package from a Ducknest instance.

        Cached metadata is returned if available. Otherwise, metadata is fetched remotely and cached.

        :param instance_url: The base URL of the Ducknest instance.
        :type instance_url: quackpack.fetcher.api_types.URLType
        :param package: The package to retrieve metadata for.
        :type package: quackpack.fetcher.api_types.Package
        :return: The resulting metadata.
        :rtype: quackpack.fetcher.api_types.SingleMetadataResult
        """

        if (result := self.cache.get_metadata(package)) is not None:
            return SingleMetadataResult(package, instance_url, result)

        result = None

        with suppress(QuackPackError):
            result = await self.ducknest_client.get_package_metadata(str(instance_url), package)
            self.cache.add_metadata(package, result)

        return SingleMetadataResult(package, instance_url, result)

    async def get_package_all_metadata(
        self, instance_url: URLType, package_name: PackageName
    ) -> MultiMetadataResult:
        """
        Retrieve metadata for all versions of a package from a Ducknest instance.

        :param instance_url: The base URL of the Ducknest instance.
        :type instance_url: quackpack.fetcher.api_types.URLType
        :param package_name: The name of the package.
        :type package_name: quackpack.fetcher.api_types.PackageName
        :return: All available metadata for the package.
        :rtype: quackpack.fetcher.api_types.MultiMetadataResult
        """

        result = None

        with suppress(QuackPackError):
            result = await self.ducknest_client.get_package_all_metadata(str(instance_url), package_name)
            self.cache.add_multi_metadata(package_name, result)

        return MultiMetadataResult(package_name, instance_url, result)

    async def publish_package(self, instance_url: str, source: Package) -> None:
        """
        Package and publish a package to a Ducknest instance.

        :param instance_url: The URL of the target Ducknest instance.
        :type instance_url: builtins.str
        :param package: The package identifier to publish.
        :type package: quackpack.fetcher.api_types.Package
        :param destination: Path to the directory containing the package to publish.
        :type destination: pathlib.Path
        """

        metadata = source.manifest
        package = FetcherPackage(id=metadata.summary.name, version=str(metadata.summary.version))

        source_files = source.files_for_publish()

        prefix = f"{package.id}-{package.version}"
        compressed_source_path = Path(mktemp(dir=self.artifacts_cache_path, prefix=prefix, suffix=".tar.gz"))

        pack_to_file(source=source_files, destination=compressed_source_path)

        with suppress(QuackPackError):
            _ = await self.ducknest_client.publish_package(
                str(instance_url), metadata.summary.into_schema(), compressed_source_path
            )

    async def get_package_blob(self, instance_url: URLType, package: FetcherPackage) -> Path | None:
        """
        Download and cache the source blob (tarball) for a given package.

        :param instance_url: The base URL of the Ducknest instance.
        :type instance_url: quackpack.fetcher.api_types.URLType
        :param package: The package to fetch.
        :type package: quackpack.fetcher.api_types.Package
        :return: Path to the cached blob file, or ``None`` if download fails.
        :rtype: pathlib.Path | None
        """

        destination = (
            self.download_cache_path / str(package.id) / package.version / self.DEFAULT_BLOB_FILENAME
        )

        if destination.exists():
            return destination

        destination.parent.mkdir(parents=True, exist_ok=True)

        result = None

        with suppress(QuackPackError):
            success = await self.ducknest_client.get_package_blob(
                str(instance_url), package, destination, self.curl_client_progress
            )
            if success:
                result = destination

        return result

    async def search(self, instance_url: str, query: str) -> SearchResult:
        """
        Search for packages on Ducknest instance matching given query.

        :param str instance_url: The base URL of the Ducknest instance.
        :param str query: Search query to be sent.
        :return: Search result obtained from the server.
        :rtype: quackpack.fetcher.api_types.SearchResult
        """

        result = SearchResult(result=[])

        with suppress(QuackPackError):
            result = await self.ducknest_client.search(str(instance_url), query)

        return result

    async def clone_from_git(self, git_entry: GitSource) -> SingleGitMetadataResult:
        """
        Clone a Git repository using the provided Git entry.

        The repository is cloned into a temporary cache directory.

        :param git_entry: Git configuration including URL and optional ref.
        :type git_entry: quackpack.config.package.GitEntry
        :return: Metadata describing the cloned package.
        :rtype: quackpack.fetcher.api_types.SingleGitMetadataResult
        """

        destination = Path(mkdtemp(dir=self.download_cache_path, prefix="git"))

        commit_hash, result = "", None

        with suppress(QuackPackError):
            commit_hash, result = await self.git_client.clone(
                ctx=self.ctx,
                url=git_entry.git_url,
                destination=destination,
                branch=git_entry.branch,
                tag=git_entry.tag,
                rev=git_entry.commit,
                progress=GitRemoteProgress(self.git_progress),
            )

        return SingleGitMetadataResult(git_entry, destination, commit_hash, result)


class FetcherContext(AbstractContextManager[Fetcher]):
    """
    A class for managing HTTP and Git clients, and caching metadata.

    :param ctx: The global context containing configuration for cache paths.
    :type ctx: quackpack.util.global_context.GlobalContext
    """

    DEFAULT_BLOB_FILENAME: Final[str] = "source.tar.gz"

    def __init__(self, ctx: GlobalContext):
        self._stack: ExitStack = ExitStack()

        self.ctx = ctx

    @override
    def __enter__(self) -> Fetcher:
        ducknest_client = self._stack.enter_context(DucknestClientContext())
        cache = self._stack.enter_context(MetadataCacheContext(self.ctx.ensure_metadata_db()))
        git_client = GitClient()
        blobs_progress = CurlProgress.make_blob_progress(self.ctx.console)
        total_progress = CurlProgress.make_completed_progress(self.ctx.console)
        bundled_progress = console.Group(blobs_progress, total_progress)
        git_progress = GitRemoteProgress.make_git_progress(self.ctx.console)

        return Fetcher(
            self.ctx,
            ducknest_client,
            cache,
            git_client,
            Progresses(
                blobs=blobs_progress, total=total_progress, bundled=bundled_progress, git=git_progress
            ),
        )

    @override
    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: TracebackType | None,
    ) -> bool | None:
        self._stack.__exit__(exc_type, exc_value, traceback)
