"""
Module for interacting with Ducknest instances.

Provides classes to fetch package metadata and blobs, and manage
the lifecycle of DucknestClient instances.

Classes
-------
DucknestClient
    Client for fetching package metadata and blobs from Ducknest instances.

DucknestClientContext
    Context manager for managing the lifecycle of a DucknestClient instance.
"""

from contextlib import AbstractContextManager
from io import BytesIO, FileIO
from pathlib import Path
from types import TracebackType
from typing import override
from urllib.parse import urljoin

from pydantic import ValidationError

from quackpack.core.fetcher.api_types import (
    MultiMetadata,
    Package,
    PackageName,
    SearchResult,
    SingleMetadata,
)
from quackpack.core.fetcher.client.curl_http_client import CurlHTTPClient
from quackpack.core.fetcher.ducknest_endpoints import DucknestEndpoints
from quackpack.core.fetcher.util import FailedRequestError, HTTPRequest, HTTPResponse
from quackpack.core.fetcher.util.curl_progress import CurlProgress
from quackpack.core.types.manifest.schemas.registry import RegistryManifestSchema
from quackpack.util.logger import get_logger
from quackpack.util.types.errors import QuackPackError
from quackpack.util.types.pkgid import Identifier

logger = get_logger(__name__)


class DucknestClient:
    """
    A client for interacting with Ducknest instances.
    """

    def __init__(self):
        self.client = CurlHTTPClient()

    def close(self):
        """
        Close the underlying HTTP client connection.
        """

        self.client.close()

    async def _get(
        self,
        url: str,
        buffer: BytesIO | FileIO,
        headers: dict[str, str] | None = None,
        curl_progress: CurlProgress | None = None,
    ) -> HTTPResponse | None:
        """
        Perform an asynchronous HTTP GET request.

        :param str url: The URL to fetch.
        :param io.BytesIO | io.FileIO buffer: A buffer to store the response body.
        :param dict[str, str] | None headers: Optional HTTP headers.
        :return: The HTTP response or None if an error occurred.
        :rtype: quackpack.fetcher.util.HTTPResponse | None
        """

        logger.debug(f"Ducknest client: GET to '{url}'")
        request = HTTPRequest(url, method="GET", headers=headers, validate_cert=False)
        response = await self.client.fetch(
            request, buffer=buffer, progress=curl_progress
        )
        return response

    async def _post(
        self,
        url: str,
        buffer: BytesIO | FileIO,
        body: bytes | None = None,
        headers: dict[str, str] | None = None,
    ) -> HTTPResponse | None:
        """
        Perform an asynchronous HTTP POST request.

        :param str url: The URL to post to.
        :param io.BytesIO | io.FileIO buffer: A buffer to store the response body.
        :param bytes | None body: Optional request body.
        :param dict[str, str] | None headers: Optional HTTP headers.
        :return: The HTTP response or None if an error occurred.
        :rtype: quackpack.fetcher.util.HTTPResponse | None
        """

        logger.debug(f"Ducknest client: POST to '{url}'")
        request = HTTPRequest(
            url, method="POST", body=body, headers=headers, validate_cert=False
        )
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
        Perform an asynchronous HTTP PUT request.

        :param str url: The URL to put to.
        :param io.BytesIO | io.FileIO buffer: A buffer to store the response body.
        :param bytes | None body: Optional request body.
        :param dict[str, str] | None headers: Optional HTTP headers.
        :return: The HTTP response or None if an error occurred.
        :rtype: quackpack.fetcher.util.HTTPResponse | None
        """

        logger.debug(f"Ducknest client: PUT to '{url}'")
        request = HTTPRequest(
            url, method="PUT", body=body, headers=headers, validate_cert=False
        )
        response = await self.client.fetch(request, buffer=buffer)
        return response

    async def get_package_metadata(
        self, instance_url: str, package: Package
    ) -> SingleMetadata:
        """
        Retrieve metadata for a specific package from a Ducknest instance.

        :param str instance_url: The base URL of the Ducknest instance.
        :param quackpack.fetcher.api_types.Package package: The package to retrieve metadata for.
        :return: The package metadata.
        :rtype: quackpack.fetcher.api_types.SingleMetadata
        :raises quackpack.fetcher.util.FailedRequestError: If the request failed.
        :raises quackpack.util.errors.QuackPackError: If the metadata cannot be decoded from JSON.
        """

        logger.debug(
            f"Ducknest client: fetching '{package.id}v{package.version!s}' metadata from '{instance_url}'"
        )
        url = urljoin(instance_url, DucknestEndpoints.get_package_single(package))

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

    async def get_package_all_metadata(
        self, instance_url: str, package_id: PackageName
    ) -> MultiMetadata:
        """
        Retrieve all metadata for a specific package from a Ducknest instance.

        :param str instance_url: The base URL of the Ducknest instance.
        :param quackpack.fetcher.api_types.PackageName package_id: The package identifier.
        :return: All metadata for the package.
        :rtype: quackpack.fetcher.api_types.MultiMetadata
        :raises quackpack.fetcher.util.FailedRequestError: If the request failed.
        :raises quackpack.util.errors.QuackPackError: If the metadata cannot be decoded from JSON.
        """

        logger.debug(
            f"Ducknest client: fetching '{package_id}' metadata tree from '{instance_url}'"
        )
        url = urljoin(instance_url, DucknestEndpoints.get_package_multi(package_id))

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

    async def publish_package(
        self, instance_url: str, manifest: RegistryManifestSchema, data_path: Path
    ) -> None:
        """
        Publish a package to a Ducknest instance.

        :param str instance_url: The base URL of the Ducknest instance.
        :param quackpack.fetcher.api_types.SingleMetadata metadata: The package metadata to publish.
        :param pathlib.Path data_path: Path to the package data file to upload.
        :raises quackpack.fetcher.util.FailedRequestError: If any of the requests fail.
        :raises quackpack.util.errors.QuackPackError: If there are validation errors.
        """

        logger.debug(
            f"Ducknest client: publishing package '{manifest.metadata.name}v{manifest.metadata.version}' to '{instance_url}'"
        )

        package = Package(
            id=Identifier(manifest.metadata.name),
            version=manifest.metadata.version.root,
        )

        create_url = urljoin(instance_url, DucknestEndpoints.post_package())
        with BytesIO() as buffer:
            response = await self._post(
                create_url,
                buffer,
                body=manifest.model_dump_json().encode(),
                headers={"Content-Type": "application/json"},
            )

            if response is None or response.error is not None:
                raise FailedRequestError

        upload_url = urljoin(instance_url, DucknestEndpoints.put_package_blob(package))

        boundary = "--------DuckNestBoundary"
        headers = {"Content-Type": f"multipart/form-data; boundary={boundary}"}

        with FileIO(data_path, "rb") as file:
            file_content = file.read()

        part_header = (
            f"--{boundary}\r\n"
            f'Content-Disposition: form-data; name="package_source"; filename="{data_path.name}"\r\n'
            "Content-Type: application/octet-stream\r\n\r\n"
        ).encode()
        part_footer = f"\r\n--{boundary}--\r\n".encode()
        multipart_body = part_header + file_content + part_footer

        with BytesIO() as buffer:
            response = await self._put(
                upload_url, buffer, body=multipart_body, headers=headers
            )

            if response is None or response.error is not None:
                raise FailedRequestError

    async def get_package_blob(
        self,
        instance_url: str,
        package: Package,
        filepath: Path,
        curl_progress: CurlProgress | None,
    ) -> bool:
        """
        Download a package blob from a Ducknest instance and save it to a file.

        :param str instance_url: The base URL of the Ducknest instance.
        :param quackpack.fetcher.api_types.Package package: The package to download the blob for.
        :param pathlib.Path filepath: The path to save the downloaded blob.
        :param quackpack.fetcher.util.CurlProgress | None curl_progress: Class for tracking CurlHTTPClient download progress.
        :return: Whether the blob was successfully saved to the specified filepath.
        :rtype: bool
        :raises quackpack.fetcher.util.FailedRequestError: If the request failed.
        """

        logger.debug(
            f"Ducknest client: fetching '{package.id}v{package.version!s}' blob from '{instance_url}' to '{filepath}'"
        )
        url = urljoin(instance_url, DucknestEndpoints.get_package_blob(package))

        with FileIO(filepath, "wb") as buffer:
            response = await self._get(url, buffer, curl_progress=curl_progress)
            if response is None:
                raise FailedRequestError

        # NOTE: This check should be a little bit more sophisticated.
        return response.code == 200

    async def search(self, instance_url: str, query: str) -> SearchResult:
        """
        Search the Ducknest instance for all packages that match the provided query.

        :param str instance_url: The base URL of the Ducknest instance.
        :param str query: Search query to be sent.
        :return: Search result obtained from the server.
        :rtype: quackpack.fetcher.api_types.SearchResult
        :raises quackpack.fetcher.util.FailedRequestError: If the request failed.
        :raises quackpack.util.errors.QuackPackError: If the result cannot be decoded from JSON.
        """

        logger.debug(f"Ducknest client: searching '{query}' on '{instance_url}'")
        url = urljoin(instance_url, DucknestEndpoints.get_search(query))

        with BytesIO() as buffer:
            response = await self._get(url, buffer)
            if response is None:
                raise FailedRequestError

            body = response.body

        try:
            search_result = SearchResult.model_validate_json(body)
            return search_result
        except ValidationError as e:
            raise QuackPackError(e) from e


class DucknestClientContext(AbstractContextManager[DucknestClient]):
    """
    Context manager for DucknestClient to manage its lifecycle.
    """

    def __init__(self):
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
