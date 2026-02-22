"""
Module for caching package metadata in an SQLite database.
----
Classes:
- `MetadataCache`: A cache for storing and retrieving package metadata using an SQLite database.
- `MetadataCacheContext`: A context manager for managing the lifecycle of a `MetadataCache` instance.
"""

from enum import StrEnum
from pathlib import Path
from typing import override

from pydantic import ValidationError

from quackpack.core.fetcher.api_types import MultiMetadata, Package, PackageName, SingleMetadata
from quackpack.core.fetcher.cache.sqlite_database import SQLiteDatabase, SQLiteDatabaseContext
from quackpack.util.logger import get_logger
from quackpack.util.types.errors import QuackPackError

logger = get_logger(__name__)


class MetadataCache(SQLiteDatabase):
    """
    A cache for storing and retrieving package metadata using an SQLite database.

    :param pathlib.Path db_path: Path to the SQLite database file.
    """

    class SQLiteMetadata:
        TABLE = "packages_metadata"

        class Columns(StrEnum):
            NAME = "name"
            VERSION = "version"
            METADATA = "metadata"

    def __init__(self, db_path: Path) -> None:
        logger.debug(f"Initializing MetadataCache at '{db_path}'")
        super().__init__(db_path)
        self.initialize()

    def initialize(self) -> None:
        """
        Initialize the SQLite database.

        Creates the ``packages_metadata`` table if it does not already exist.
        """

        logger.debug("Creating SQLite backend database")
        query = f"""
        CREATE TABLE IF NOT EXISTS {self.SQLiteMetadata.TABLE} (
            {self.SQLiteMetadata.Columns.NAME} TEXT NOT NULL,
            {self.SQLiteMetadata.Columns.VERSION} TEXT NOT NULL,
            {self.SQLiteMetadata.Columns.METADATA} TEXT,
            PRIMARY KEY ({self.SQLiteMetadata.Columns.NAME}, {self.SQLiteMetadata.Columns.VERSION})
        );
        """
        self.execute_query(query)

    def get_metadata(self, package: Package) -> SingleMetadata | None:
        """
        Retrieve metadata for a specific package from the cache.

        :param quackpack.fetcher.api_types.Package package: The package to retrieve metadata for.
        :return: The corresponding metadata if found, otherwise None.
        :rtype: quackpack.fetcher.api_types.SingleMetadata | None
        :raises quackpack.util.errors.QuackPackError: If metadata is invalid or cannot be parsed.
        """

        logger.debug(f"Looking for '{package.id}v{package.version!s}'")
        query = f"""
            SELECT {self.SQLiteMetadata.Columns.METADATA}
            FROM {self.SQLiteMetadata.TABLE}
            WHERE {self.SQLiteMetadata.Columns.NAME} = ? AND {self.SQLiteMetadata.Columns.VERSION} = ?
        """
        result = self.fetch_one(query, (str(package.id), str(package.version)))

        if result is None:
            logger.debug(f"Could not find '{package.id}v{package.version}' in MetadataCache")
            return None

        try:
            metadata = SingleMetadata.model_validate_json(result[0])
        except ValidationError as e:
            raise QuackPackError(e) from e

        return metadata

    def get_multi_metadata(self, package_id: PackageName) -> MultiMetadata | None:
        """
        Retrieve metadata for all versions of a given package.

        :param quackpack.fetcher.api_types.PackageName package_id: The package name to retrieve metadata for.
        :return: A collection of metadata for all known versions, or None if not found.
        :rtype: quackpack.fetcher.api_types.MultiMetadata | None
        :raises quackpack.util.errors.QuackPackError: If metadata is invalid or cannot be parsed.
        """

        logger.debug(f"Looking for '{package_id}'")
        query = f"SELECT {self.SQLiteMetadata.Columns.METADATA} FROM {self.SQLiteMetadata.TABLE} WHERE {self.SQLiteMetadata.Columns.NAME} = ?"
        results = self.fetch_all(query, (str(package_id),))

        try:
            multi_metadata = MultiMetadata(
                packages_metadata=[SingleMetadata.model_validate_json(result[0]) for result in results]
            )
        except ValidationError as e:
            raise QuackPackError(e) from e

        return multi_metadata

    def add_metadata(self, package: Package, metadata: SingleMetadata) -> None:
        """
        Add or update metadata for a specific package in the cache.

        :param quackpack.fetcher.api_types.Package package: The package identifier (name and version).
        :param quackpack.fetcher.api_types.SingleMetadata metadata: The metadata to store.
        :return: None
        :rtype: None
        """

        logger.debug(f"Adding '{package.id}v{package.version!s}' with {metadata=}")
        query = f"""
            INSERT OR REPLACE INTO {self.SQLiteMetadata.TABLE} (
                {self.SQLiteMetadata.Columns.NAME},
                {self.SQLiteMetadata.Columns.VERSION},
                {self.SQLiteMetadata.Columns.METADATA}
            ) VALUES (?, ?, ?)
        """

        metadata_string = metadata.model_dump_json()
        self.execute_query(query, (str(package.id), str(package.version), metadata_string))

    def add_multi_metadata(self, multi_package_id: PackageName, multi_metadata: MultiMetadata) -> None:
        """
        Add or update metadata for multiple versions of a package in the cache.

        :param quackpack.fetcher.api_types.PackageName multi_package_id: The package name for which metadata is being stored.
        :param quackpack.fetcher.api_types.MultiMetadata multi_metadata: A collection of metadata for different versions of the package.
        :return: None
        :rtype: None
        """

        for metadata in multi_metadata.packages_metadata:
            assert metadata.metadata is not None, "should come from parse"
            assert metadata.metadata.version is not None, "should come from parse"
            package = Package(id=multi_package_id, version=metadata.metadata.version.root)

            self.add_metadata(package, metadata)


class MetadataCacheContext(SQLiteDatabaseContext[MetadataCache]):
    """
    A context manager for managing the lifecycle of a MetadataCache instance.

    Ensures the underlying SQLite database is opened and closed properly when used
    within a context manager block.

    :ivar MetadataCache ducknest_client: The internal MetadataCache instance managed by the context.
    """

    @override
    def _create_instance(self) -> MetadataCache:
        return MetadataCache(self.db_path)
