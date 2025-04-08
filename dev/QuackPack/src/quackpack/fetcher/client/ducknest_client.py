"""
Module for interacting with Ducknest instances.
----
Classes:
- `DucknestClient`: A client for fetching package metadata and blobs from Ducknest instances.
- `DucknestClientContext`: A context manager for managing the lifecycle of a DucknestClient instance.
"""

from contextlib import AbstractContextManager
from io import BytesIO, FileIO
from pathlib import Path
from types import TracebackType
from typing import override
from urllib.parse import urljoin

from pydantic import ValidationError

from quackpack.fetcher.api_types import MultiMetadata, Package, SingleMetadata
from quackpack.fetcher.client.curl_http_client import CurlHTTPClient
from quackpack.fetcher.ducknest_endpoints import DucknestEndpoints
from quackpack.fetcher.util import FailedRequestError, HTTPRequest, HTTPResponse
from quackpack.util.errors import QuackPackError
from quackpack.util.logger import get_logger

logger = get_logger(__name__)


class DucknestClient:
    """
    A client for interacting with Ducknest instances.
    """

    def __init__(self):
        self.client = CurlHTTPClient()

    def close(self):
        """
        Close HTTP client.
        """

        self.client.close()

    async def _get(
        self, url: str, buffer: BytesIO | FileIO, headers: dict[str, str] | None = None
    ) -> HTTPResponse | None:
        """
        Make a GET request using the HTTP client.
        ----
        Args:
        - `url`: The URL to fetch.
        - `buffer`: Buffer to which response body will be saved.
        - `headers`: Optional request headers.
        ----
        Returns:
        - `HTTPResponse | None`: The HTTP response, or None if an error occurred.
        """

        logger.debug(f"Ducknest client: GET to '{url}'")
        request = HTTPRequest(url, method="GET", headers=headers, validate_cert=False)
        response = await self.client.fetch(request, buffer=buffer)
        return response

    async def _post(
        self,
        url: str,
        buffer: BytesIO | FileIO,
        body: bytes | None = None,
        headers: dict[str, str] | None = None,
    ) -> HTTPResponse | None:
        """
        Make a POST request using the HTTP client.
        ----
        Args:
        - `url`: The URL to post to.
        - `buffer`: Buffer to which response body will be saved.
        - `body`: Optional request body.
        - `headers`: Optional request headers.
        ----
        Returns:
        - `HTTPResponse | None`: The HTTP response, or None if an error occurred.
        """
        logger.debug(f"Ducknest client: POST to '{url}'")
        request = HTTPRequest(url, method="POST", body=body, headers=headers, validate_cert=False)
        response = await self.client.fetch(request, buffer=buffer)
        return response

    async def _put(
        self,
        url: str,
        buffer: BytesIO | FileIO,
        body: bytes | None = None,
        headers: dict[str, str] | None = None,
    ) -> HTTPResponse | None:
        """
        Make a PUT request using the HTTP client.
        ----
        Args:
        - `url`: The URL to put to.
        - `buffer`: Buffer to which response body will be saved.
        - `body`: Optional request body.
        - `headers`: Optional request headers.
        ----
        Returns:
        - `HTTPResponse | None`: The HTTP response, or None if an error occurred.
        """
        logger.debug(f"Ducknest client: PUT to '{url}'")
        request = HTTPRequest(url, method="PUT", body=body, headers=headers, validate_cert=False)
        response = await self.client.fetch(request, buffer=buffer)
        return response

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
        - `FailedRequestError`: if request failed.
        - `QuackPackError`: If the metadata cannot be decoded from JSON.
        """
        logger.debug(
            f"Ducknest client: fetching '{package.name}v{package.version!s}' metadata from '{instance_url}'"
        )
        url = urljoin(instance_url, DucknestEndpoints.package_version(package))

        with BytesIO() as buffer:
            response = await self._get(url, buffer)
            if response is None:
                raise FailedRequestError

            body = response.body

        try:
            metadata = SingleMetadata.model_validate_json(body)
            return metadata
        except ValidationError as e:
            raise QuackPackError(e) from e

    async def get_package_all_metadata(self, instance_url: str, package: Package) -> MultiMetadata:
        """
        Retrieve all metadata for a specific package from a Ducknest instance.
        ----
        Args:
        - `instance_url`: The base URL of the Ducknest instance.
        - `package`: The package to retrieve metadata for.
        ----
        Returns:
        - `MultiMetadata`: The package metadata.
        ----
        Raises:
        - `FailedRequestError`: if request failed.
        - `QuackPackError`: If the metadata cannot be decoded from JSON.
        """

        logger.debug(
            f"Ducknest client: fetching '{package.name}v{package.version!s}' metadata tree from '{instance_url}'"
        )
        url = urljoin(instance_url, DucknestEndpoints.package(package))

        with BytesIO() as buffer:
            response = await self._get(url, buffer)
            if response is None:
                raise FailedRequestError

            body = response.body

        try:
            metadata = MultiMetadata.model_validate_json(body)
            return metadata
        except ValidationError as e:
            raise QuackPackError(e) from e

    # TODO: adapt signature to actual implementation (interface)
    async def publish_package(
        self, instance_url: str, metadata: SingleMetadata, source_file: Path | None = None
    ) -> None:
        """
        Publish a package to a Ducknest instance.
        ----
        Args:
        - `instance_url`: The base URL of the Ducknest instance.
        - `metadata`: The package metadata to publish.
        - `source_file`: Optional path to the source file to upload.
        ----
        Raises:
        - `FailedRequestError`: If any of the requests fail.
        - `QuackPackError`: If there are validation errors.
        """

        logger.debug(
            f"Ducknest client: publishing package '{metadata.metadata.name}v{metadata.metadata.version}' to '{instance_url}'"
        )

        package = Package(name=metadata.metadata.name, version=str(metadata.metadata.version))

        create_url = urljoin(instance_url, DucknestEndpoints.package(package))
        with BytesIO() as buffer:
            response = await self._post(
                create_url,
                buffer,
                body=metadata.model_dump_json().encode(),
                headers={"Content-Type": "application/json"},
            )

            if response is None or response.error is not None:
                raise FailedRequestError

        if source_file is not None:
            if not source_file.exists():
                raise FailedRequestError

            upload_url = urljoin(instance_url, DucknestEndpoints.blob(package))

            with FileIO(source_file, "rb") as file, BytesIO() as buffer:
                response = await self._put(
                    upload_url, buffer, body=file.read(), headers={"Content-Type": "application/octet-stream"}
                )

                if response is None or response.error is not None:
                    raise FailedRequestError

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
        ----
        Raises:
        - `FailedRequestError`: If request failed.
        """

        logger.debug(
            f"Ducknest client: fetching '{package.name}v{package.version!s}' blob from '{instance_url}' to '{filepath}'"
        )
        url = urljoin(instance_url, DucknestEndpoints.blob(package))

        with FileIO(filepath, "wb") as buffer:
            response = await self._get(url, buffer)
            # TODO: check error code and if different than 200, clear file and throw some exception
            if response is None:
                raise FailedRequestError

        return None


class DucknestClientContext(AbstractContextManager[DucknestClient]):
    def __init__(self):
        """
        A context manager for DucknestClient to manage the lifecycle of DucknestClient instances.

        This class ensures that the DucknestClient is properly opened and closed when used in a `with` statement.
        """

        self.ducknest_client: DucknestClient | None = None

    @override
    def __enter__(self) -> DucknestClient:
        self.ducknest_client = DucknestClient()
        return self.ducknest_client

    @override
    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: TracebackType | None,
    ) -> bool | None:
        if self.ducknest_client is not None:
            self.ducknest_client.close()

        return False if exc_type is not None else None
