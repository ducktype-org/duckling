from typing import Annotated

from fastapi import APIRouter, Body, Path, UploadFile
from fastapi.responses import FileResponse, JSONResponse, Response

from quackpack.fetcher.api_types import MultiMetadata, Package, SingleMetadata

from .handlers.packages_handlers import (
    create_package_handler,
    download_source_handler,
    get_metadata_handler,
    get_multi_metadata_handler,
    upload_source_handler,
    # search_handler,
)

router = APIRouter(prefix="/packages")


@router.get("/{name}/{version}/download")
async def download_source(package: Annotated[Package, Path()]) -> FileResponse:
    """
    Handles the request to download the source file for a specific package.
    """
    return download_source_handler(package)


@router.get("/{name}/{version}")
async def get_metadata(package: Annotated[Package, Path()]) -> SingleMetadata:
    """
    Handles the request to get metadata for a specific package.
    """
    return get_metadata_handler(package)


@router.get("/{name}")
async def get_multi_metadata(package: Annotated[Package, Path()]) -> MultiMetadata:
    """
    Handles the request to get metadata for a specific package.
    """
    return get_multi_metadata_handler(package)


@router.post("")
async def create_package(package_metadata: Annotated[SingleMetadata, Body()]) -> JSONResponse:
    """
    Handles the creation of a new package by saving the provided metadata.
    """
    return create_package_handler(package_metadata)


@router.put("/{name}/{version}")
async def upload_source(package: Annotated[Package, Path()], package_source: UploadFile) -> Response:
    """
    Handles the upload of a source file for an existing package.
    """
    return await upload_source_handler(package, package_source)


# TODO: implement if needed
# @router.get("")
# async def search(q: Annotated[str | None, Query()] = None) -> JSONResponse:
#     """
#     Handles the search for packages based on a query string.
#     """
#     return await search_handler(q)
