"""
Module for interacting with Ducknest instances.
----
Classes:
- `DucknestClient`: A client for fetching package metadata and blobs from Ducknest instances.
"""

import json
from json import JSONDecodeError
from pathlib import Path
from types import TracebackType
from typing import Self
from urllib.parse import urljoin

from common import DucknestEndpoints, MultiMetadata, Package, SingleMetadata
from quackpack.errors import QuackPackError
from quackpack.fetcher.client import CurlHTTPClient
from quackpack.fetcher.util import HTTPRequest, HTTPResponse


class DucknestClient:
    """
    A client for interacting with Ducknest instances.
    """

    def __init__(self):
        self.client: CurlHTTPClient | None = None

    def __enter__(self) -> Self:
        self.client = CurlHTTPClient()
        return self

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: TracebackType | None,
    ) -> bool | None:
        if self.client is None:
            return

        self.client.close()

        if exc_type is not None:
            return False

    async def _fetch(self, url: str, filepath: Path | None = None) -> HTTPResponse | None:
        """
        Fetch a URL using the HTTP client.
        ----
        Args:
        - `url`: The URL to fetch.
        - `filepath`: Optional path to save the response body to a file.
        ----
        Returns:
        - `HTTPResponse | None`: The HTTP response, or None if the client is not initialized.
        """

        if self.client is None:
            return None

        request = HTTPRequest(url, validate_cert=False)
        future_response = await self.client.fetch(request, filepath=filepath)
        return future_response.result() if future_response is not None else None

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
        ----
        Raises:
        - `QuackPackError`: If the metadata cannot be decoded from JSON.
        """

        url = urljoin(instance_url, DucknestEndpoints.metadata(package))
        response = await self._fetch(url)
        if response is None:
            return None

        body = response.body
        try:
            metadata = json.loads(body)
            return metadata
        except JSONDecodeError as e:
            raise QuackPackError(e) from e

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
        ----
        Raises:
        - `QuackPackError`: If the metadata cannot be decoded from JSON.
        """

        url = urljoin(instance_url, DucknestEndpoints.all_metadata(package))
        response = await self._fetch(url)
        if response is None:
            return None

        body = response.body
        try:
            metadata = json.loads(body)
            return metadata
        except JSONDecodeError as e:
            raise QuackPackError(e) from e

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

        url = urljoin(instance_url, DucknestEndpoints.blob(package))
        response = await self._fetch(url, filepath)
        if response is None:
            return None

        # TODO
        return None
