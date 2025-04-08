import os

from fastapi import HTTPException, UploadFile, status
from fastapi.responses import FileResponse, JSONResponse, Response
from pydantic import ValidationError

from quackpack.fetcher.api_types import MultiMetadata, Package, SingleMetadata

STATIC_DIR = "static"
PACKAGE_METADATA_FILE = "metadata.json"
PACKAGE_SOURCE_FILE = "source.tar.gz"


def package_dir(package: Package) -> str:
    """
    Returns the directory path for a given package based on its name and version.

    Args:
        package (Package): The package object containing name and version.

    Returns:
        str: The directory path for the package.
    """
    base_dir = os.environ.get("FASTAPI_BASEDIR", "./")

    return os.path.join(base_dir, STATIC_DIR, package.name, str(package.version or ""))


def download_source_handler(package: Package) -> FileResponse:
    """
    Handles the request to download the source file of a package.
    """
    package_source_path = os.path.join(package_dir(package), PACKAGE_SOURCE_FILE)

    if not os.path.exists(package_source_path):
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND)

    return FileResponse(package_source_path, filename=package_source_path)


def get_metadata_handler(package: Package) -> SingleMetadata:
    """
    Handles the request to fetch the metadata of a specific version of package.
    """
    metadata_file_path = os.path.join(package_dir(package), PACKAGE_METADATA_FILE)

    if not os.path.exists(metadata_file_path):
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND)

    with open(metadata_file_path) as file:
        content = file.read()

    try:
        metadata = SingleMetadata.model_validate_json(content)
    except ValidationError as e:
        raise e

    return metadata


def get_multi_metadata_handler(package: Package) -> MultiMetadata:
    """
    Handles the request to fetch the metadata of a package.
    """

    packages_metadata: list[SingleMetadata] = []
    packages_metadata_dir = package_dir(package)

    if not os.path.exists(packages_metadata_dir):
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND)

    for subpath in sorted(os.listdir(packages_metadata_dir)):
        # TODO: assume subpath is Version type
        versioned_package = Package(name=package.name, version=subpath)

        versioned_package_metadata_path = os.path.join(package_dir(versioned_package), PACKAGE_METADATA_FILE)

        if not os.path.exists(versioned_package_metadata_path):
            continue

        with open(versioned_package_metadata_path) as file:
            content = file.read()

        try:
            metadata = SingleMetadata.model_validate_json(content)
        except ValidationError:
            continue

        packages_metadata.append(metadata)

    return MultiMetadata(packages_metadata=packages_metadata)


def create_package_handler(package_metadata: SingleMetadata) -> JSONResponse:
    """
    Handles the request to create a new package by saving the provided metadata.

    Args:
        package_metadata (SingleMetadata): The metadata of the package to be created.

    Raises:
        HTTPException: If the package already exists.

    Returns:
        JSONResponse: The response containing the package metadata and a location header.
    """

    package = Package(name=package_metadata.metadata.name, version=str(package_metadata.metadata.version))
    package_dir_path = package_dir(package)

    if os.path.exists(package_dir_path):
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Package already exists")

    os.makedirs(package_dir_path)

    metadata_file_path = os.path.join(package_dir_path, PACKAGE_METADATA_FILE)

    with open(metadata_file_path, "w") as file:
        content = package_metadata.model_dump_json()
        file.write(content)

    package_location = f"/{package.name}/{package.version}"

    # TODO: response with proper URL location header (to check)
    return JSONResponse(
        content=package_metadata.model_dump(),
        status_code=status.HTTP_201_CREATED,
        headers={"Location": package_location},
    )


async def upload_source_handler(package: Package, package_source: UploadFile) -> Response:
    """
    Handles the upload of a source file for a given package.

    Args:
        package (Package): The package object to which the source file is uploaded.
        package_source (UploadFile): The source file being uploaded.

    Raises:
        HTTPException: If the package is not found or if the source file already exists.

    Returns:
        Response: The response indicating the result of the upload operation.
    """
    package_dir_path = package_dir(package)
    source_file_path = os.path.join(package_dir_path, PACKAGE_SOURCE_FILE)

    if not os.path.exists(package_dir_path):
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Package not found")

    if os.path.exists(source_file_path):
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Package already has source")

    # TODO: should we verify whether source metadata is the same as uploaded metadata?
    with open(source_file_path, "wb") as file:
        content = await package_source.read()
        file.write(content)

    return Response(status_code=status.HTTP_204_NO_CONTENT)


# TODO: implement if needed
# async def search_handler(q: str | None) -> JSONResponse:
#     """
#     Handles the request to search for packages based on the provided query string.
#
#     Args:
#         q (str | None): The search query string to filter packages by name.
#
#     Returns:
#         JSONResponse: The response containing a list of packages matching the search query.
#     """
#     packages = []
#
#     for root, _, filenames in os.walk(STATIC_DIR):
#         for filename in filenames:
#             if filename == PACKAGE_METADATA_FILE:
#                 with open(os.path.join(root, filename)) as file:
#                     content = file.read()
#                 metadata = SingleMetadata.model_validate_json(content)
#                 if not q or q.lower() in metadata.metadata.name.lower():
#                     packages.append(metadata.model_dump_json())
#
#     return JSONResponse(content=packages)
