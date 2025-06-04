"""
Module for fetching package metadata and blobs from Ducknest instances and cloning Git repositories.
----
Classes:
- `Fetcher`: A class for managing HTTP and Git clients, and caching metadata.
"""

from contextlib import ExitStack, suppress
from pathlib import Path
from tempfile import mkdtemp, mktemp
from types import TracebackType
from typing import Final, Self, cast

from quackpack.config.project import GitEntry
from quackpack.fetcher.api_types import (
    MultiMetadataResult,
    Package,
    PackageName,
    SearchResult,
    SingleGitMetadataResult,
    SingleMetadataResult,
    URLType,
)
from quackpack.fetcher.cache import MetadataCache, MetadataCacheContext
from quackpack.fetcher.client import DucknestClient, DucknestClientContext, GitClient
from quackpack.fetcher.util.errors import UninitializedClientError
from quackpack.project import Project
from quackpack.util.compression import pack_to_file
from quackpack.util.errors import QuackPackError
from quackpack.util.global_context import GlobalContext
from quackpack.util.lock import LockType
from quackpack.util.logger import get_logger

logger = get_logger(__name__)


class Fetcher:
    """
    A class for managing HTTP and Git clients, and caching metadata.

    :param ctx: The global context containing configuration for cache paths.
    :type ctx: quackpack.util.global_context.GlobalContext
    """

    DEFAULT_BLOB_FILENAME: Final[str] = "source.tar.gz"

    # FIXME(stach): Czy instance_url powinien być przypisany w konstruktorze z ctx?
    def __init__(self, ctx: GlobalContext):
        self._stack: ExitStack

        self.metadata_cache_db_path = ctx.configuration.cache.metadata_db_path
        self.download_cache_path = ctx.configuration.cache.download_dir
        self.artifacts_cache_path = ctx.configuration.cache.artifacts_dir

        logger.debug(
            f"Initializing fetcher with {self.metadata_cache_db_path=} and {self.download_cache_path=}"
        )

        self.ducknest_client: DucknestClient | None = None
        self.cache: MetadataCache | None = None
        self.git_client = GitClient()

    def __enter__(self) -> Self:
        self._stack = ExitStack()

        self.ducknest_client = self._stack.enter_context(DucknestClientContext())
        self.cache = self._stack.enter_context(MetadataCacheContext(self.metadata_cache_db_path))

        return self

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: TracebackType | None,
    ) -> bool | None:
        self._stack.__exit__(exc_type, exc_value, traceback)

    async def get_package_metadata(self, instance_url: URLType, package: Package) -> SingleMetadataResult:
        """
        Retrieve metadata for a specific package from a Ducknest instance.

        Cached metadata is returned if available. Otherwise, metadata is fetched remotely and cached.

        :param instance_url: The base URL of the Ducknest instance.
        :type instance_url: quackpack.fetcher.api_types.URLType
        :param package: The package to retrieve metadata for.
        :type package: quackpack.fetcher.api_types.Package
        :return: The resulting metadata.
        :rtype: quackpack.fetcher.api_types.SingleMetadataResult
        :raises quackpack.fetcher.util.UninitializedClientError: If the fetcher has not been initialized.
        """

        if self.ducknest_client is None or self.cache is None:
            raise UninitializedClientError

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
        :raises quackpack.fetcher.util.UninitializedClientError: If the fetcher has not been initialized.
        """

        # TODO: If we support offline mode, then check in the cache only.
        #       Otherwise we should always make request, because there could be new version.

        if self.ducknest_client is None or self.cache is None:
            raise UninitializedClientError

        result = None

        with suppress(QuackPackError):
            result = await self.ducknest_client.get_package_all_metadata(str(instance_url), package_name)
            self.cache.add_multi_metadata(package_name, result)

        return MultiMetadataResult(package_name, instance_url, result)

    async def publish_package(self, instance_url: str, source: Project) -> None:
        """
        Package and publish a project to a Ducknest instance.

        :param instance_url: The URL of the target Ducknest instance.
        :type instance_url: builtins.str
        :param package: The package identifier to publish.
        :type package: quackpack.fetcher.api_types.Package
        :param destination: Path to the directory containing the project to publish.
        :type destination: pathlib.Path
        :raises quackpack.fetcher.util.UninitializedClientError: If the fetcher has not been initialized.
        """

        if self.ducknest_client is None or self.cache is None:
            raise UninitializedClientError

        metadata = source.manifest_with_acquiring_lock(locktype=LockType.SHARED)
        package = Package(id=metadata.metadata.name, version=str(metadata.metadata.version))

        source_files = source.files_for_publish()

        prefix = f"{package.id}-{package.version}"
        compressed_source_path = Path(mktemp(dir=self.artifacts_cache_path, prefix=prefix, suffix=".tar.gz"))

        pack_to_file(source=source_files, destination=compressed_source_path)

        # TODO: do we want another type denoting success or failure???
        with suppress(QuackPackError):
            _ = await self.ducknest_client.publish_package(
                str(instance_url), metadata, compressed_source_path
            )

    async def get_package_blob(self, instance_url: URLType, package: Package) -> Path | None:
        """
        Download and cache the source blob (tarball) for a given package.

        :param instance_url: The base URL of the Ducknest instance.
        :type instance_url: quackpack.fetcher.api_types.URLType
        :param package: The package to fetch.
        :type package: quackpack.fetcher.api_types.Package
        :return: Path to the cached blob file, or ``None`` if download fails.
        :rtype: pathlib.Path | None
        :raises quackpack.fetcher.util.UninitializedClientError: If the fetcher has not been initialized.
        """

        if self.ducknest_client is None or self.cache is None:
            raise UninitializedClientError

        # TODO: review - probably add to some global utils

        # TODO brakuje źródła.
        # Trzeba to ustalić: najwygodniej pewnie mieć wspólną logikę do storage'a:
        # użyć ResolvedId i tam już ustalić jak to ma się stringować, ale nie jestem pewien
        # czy fetcher działa w tym kontekście tutaj tak jak myślę (Artur)
        destination = (
            self.download_cache_path / str(package.id) / package.version / self.DEFAULT_BLOB_FILENAME
        )

        if destination.exists():
            return destination

        destination.parent.mkdir(parents=True, exist_ok=True)

        result = None

        with suppress(QuackPackError):
            success = await self.ducknest_client.get_package_blob(str(instance_url), package, destination)
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
        :raises quackpack.fetcher.util.UninitializedClientError: If the fetcher has not been initialized.
        """

        if self.ducknest_client is None or self.cache is None:
            raise UninitializedClientError

        result = SearchResult(result=[])

        with suppress(QuackPackError):
            result = await self.ducknest_client.search(str(instance_url), query)

        return result

    async def clone_from_git(self, git_entry: GitEntry) -> SingleGitMetadataResult:
        """
        Clone a Git repository using the provided Git entry.

        The repository is cloned into a temporary cache directory.

        :param git_entry: Git configuration including URL and optional ref.
        :type git_entry: quackpack.config.project.GitEntry
        :return: Metadata describing the cloned project.
        :rtype: quackpack.fetcher.api_types.SingleGitMetadataResult
        """

        destination = Path(mkdtemp(dir=self.download_cache_path, prefix="git"))

        commit_hash, result = "", None

        with suppress(QuackPackError):
            commit_hash, result = await self.git_client.clone(
                url=cast(str, git_entry.git_url),  # TODO: finest touches
                destination=destination,
                branch=git_entry.branch,
                tag=git_entry.tag,
                rev=git_entry.commit,
            )

        return SingleGitMetadataResult(git_entry, destination, commit_hash, result)
