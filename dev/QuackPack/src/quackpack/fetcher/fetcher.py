"""
Module for fetching package metadata and blobs from Ducknest instances and cloning Git repositories.
----
Classes:
- `Fetcher`: A class for managing HTTP and Git clients, and caching metadata.
"""

from contextlib import ExitStack
from pathlib import Path
from types import TracebackType
from typing import Self

from quackpack.fetcher.api_types import MultiMetadata, Package, SingleMetadata
from quackpack.fetcher.cache import MetadataCache, MetadataCacheContext
from quackpack.fetcher.client import DucknestClient, DucknestClientContext, GitClient
from quackpack.fetcher.util.errors import UninitializedClientError
from quackpack.util.logger import get_logger

logger = get_logger(__name__)


class Fetcher:
    """
    A class for managing HTTP and Git clients, and caching metadata.
    ----
    Args:
    - `metadata_cache_db_path`: Path to the cache directory for storing metadata.
    """

    def __init__(self, metadata_cache_db_path: Path):
        logger.debug(f"Initializing fetcher with {metadata_cache_db_path=}")
        self._stack: ExitStack

        self.metadata_cache_db_path = metadata_cache_db_path

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

    async def get_package_metadata(self, instance_url: str, package: Package) -> SingleMetadata:
        """
        Retrieve metadata for a specific package from a Ducknest instance.
        ----
        Args:
        - `instance_url`: The base URL of the Ducknest instance.
        - `package`: The package to retrieve metadata for.
        ----
        Returns:
        - `SingleMetadata`: The package metadata.
        ----
        Raises:
        - `UninitializedClientError`: If client is not initialized.
        """

        if self.ducknest_client is None or self.cache is None:
            raise UninitializedClientError

        if (result := self.cache.get_metadata(package)) is not None:
            return result

        result = await self.ducknest_client.get_package_metadata(instance_url, package)
        self.cache.add_metadata(package, result)

        return result

    async def get_package_all_metadata(self, instance_url: str, package: Package) -> MultiMetadata:
        """
        Retrieve all metadata for a specific package from a Ducknest instance.
        ----
        Args:
        - `instance_url`: The base URL of the Ducknest instance.
        - `package`: The package to retrieve metadata for.
        ----
        Returns:
        - `MultiMetadata`: The packages metadata.
        ----
        Raises:
        - `UninitializedClientError`: If client is not initialized.
        """

        # TODO: If we support offline mode, then check in the cache only.
        #       Otherwise we should always make request, because there could be new version.

        if self.ducknest_client is None or self.cache is None:
            raise UninitializedClientError

        result = await self.ducknest_client.get_package_all_metadata(instance_url, package)
        self.cache.add_multi_metadata(package, result)

        return result

    async def publish_package(self, instance_url: str, package: Package, package_path: Path) -> None:
        """
        TODO
        """

        raise NotImplementedError

    # TODO: how do we validate whether fetch was successful
    async def get_package_blob(self, instance_url: str, package: Package, destination: Path) -> None:
        """
        Download a package blob from a Ducknest instance and save it to a file.
        ----
        Args:
        - `instance_url`: The base URL of the Ducknest instance.
        - `package`: The package to download the blob for.
        - `destination`: The path to save the downloaded blob.
        ----
        Raises:
        - `UninitializedClientError`: If client is not initialized.
        """

        # TODO: check in blob cache
        if self.ducknest_client is None or self.cache is None:
            raise UninitializedClientError

        # TODO: exceptions?
        result = await self.ducknest_client.get_package_blob(instance_url, package, destination)

        # TODO: add to cache
        return result

    async def clone(
        self,
        url: str,
        destination: Path,
        branch: str | None = None,
        tag: str | None = None,
        rev: str | None = None,
    ) -> None:
        """
        Clone a Git repository to the specified destination.
        ----
        Args:
        - `url`: The URL of the Git repository.
        - `destination`: The local path where the repository will be cloned.
        - `branch`: Optional branch to clone.
        - `tag`: Optional tag to clone.
        - `rev`: Optional revision (commit hash) to checkout after cloning.
        """

        result = await self.git_client.clone(url, destination, branch=branch, tag=tag, rev=rev)
        # TODO: extract metadata from result
        return result
