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

from common import MultiMetadata, Package, SingleMetadata
from quackpack.fetcher.cache import MetadataCache
from quackpack.fetcher.client import DucknestClient, GitClient


class Fetcher:
    """
    A class for managing HTTP and Git clients, and caching metadata.
    ----
    Args:
    - `metadata_cache_db_path`: Path to the cache directory for storing metadata.
    """

    def __init__(self, metadata_cache_db_path: Path):
        self._stack: ExitStack

        self.metadata_cache_db_path = metadata_cache_db_path

        self.ducknest_client: DucknestClient | None = None
        self.cache: MetadataCache | None = None
        self.git_client = GitClient()

    def __enter__(self) -> Self:
        self.exit_stack = ExitStack()

        self.ducknest_client = self.exit_stack.enter_context(DucknestClient())
        self.cache = self.exit_stack.enter_context(MetadataCache(self.metadata_cache_db_path))

        return self

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: TracebackType | None,
    ) -> bool | None:
        self.exit_stack.__exit__(exc_type, exc_value, traceback)

    async def get_package_metadata(self, instance_url: str, package: Package) -> SingleMetadata | None:
        """
        Retrieve metadata for a specific package from a Ducknest instance.
        ----
        Args:
        - `instance_url`: The base URL of the Ducknest instance.
        - `package`: The package to retrieve metadata for.
        ----
        Returns:
        - `SingleMetadata | None`: The package metadata, or None if the request fails.
        """

        # TODO: check in the cache
        if self.ducknest_client is None:
            return

        # TODO: expections?
        result = await self.ducknest_client.get_package_metadata(instance_url, package)
        return result

    async def get_package_all_metadata(self, instance_url: str, package: Package) -> MultiMetadata | None:
        """
        Retrieve all metadata for a specific package from a Ducknest instance.
        ----
        Args:
        - `instance_url`: The base URL of the Ducknest instance.
        - `package`: The package to retrieve metadata for.
        ----
        Returns:
        - `MultiMetadata | None`: The package metadata, or None if the request fails.
        """

        # TODO: check in the cache
        if self.ducknest_client is None:
            return

        # TODO: expections?
        result = await self.ducknest_client.get_package_all_metadata(instance_url, package)
        return result

    # TODO: how do we validate whether fetch was successful
    async def get_package_blob(self, instance_url: str, package: Package, filepath: Path) -> None:
        """
        Download a package blob from a Ducknest instance and save it to a file.
        ----
        Args:
        - `instance_url`: The base URL of the Ducknest instance.
        - `package`: The package to download the blob for.
        - `filepath`: The path to save the downloaded blob.
        ----
        Returns:
        - `None`: The blob is saved to the specified filepath.
        """

        if self.ducknest_client is None:
            return

        # TODO: expections?
        result = await self.ducknest_client.get_package_blob(instance_url, package, filepath)
        return result

    async def clone(
        self,
        url: str,
        filepath: Path,
        branch: str | None = None,
        tag: str | None = None,
        rev: str | None = None,
    ) -> None:
        """
        Clone a Git repository to the specified filepath.
        ----
        Args:
        - `url`: The URL of the Git repository.
        - `filepath`: The local path where the repository will be cloned.
        - `branch`: Optional branch to clone.
        - `tag`: Optional tag to clone.
        - `rev`: Optional revision (commit hash) to checkout after cloning.
        ----
        Returns:
        - `None`: The repository is cloned to the specified filepath.
        """

        # TODO: expections?
        result = await self.git_client.clone(url, filepath, branch=branch, tag=tag, rev=rev)
        # TODO: extract metadata from result
        return result
