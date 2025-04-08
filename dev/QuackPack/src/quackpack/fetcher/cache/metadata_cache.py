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

from quackpack.fetcher.api_types import MultiMetadata, Package, SingleMetadata
from quackpack.fetcher.cache.sqlite_database import SQLiteDatabase, SQLiteDatabaseContext
from quackpack.util.errors import QuackPackError
from quackpack.util.logger import get_logger

logger = get_logger(__name__)


class MetadataCache(SQLiteDatabase):
    """
    A cache for storing and retrieving package metadata using an SQLite database.
    ----
    Args:
    - `db_path`: Path to the SQLite database file.
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
        Initialize the database by creating the `packages_metadata` table if it does not exist.
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
        ----
        Args:
        - `package`: The package to retrieve metadata for.
        ----
        Returns:
        - `SingleMetadata | None`: The package metadata, or None if not found.
        ----
        Raises:
        - `QuackPackError`: If the metadata cannot be decoded from JSON.
        """

        logger.debug(f"Looking for '{package.name}v{package.version!s}'")
        query = f"""
            SELECT {self.SQLiteMetadata.Columns.METADATA}
            FROM {self.SQLiteMetadata.TABLE}
            WHERE {self.SQLiteMetadata.Columns.NAME} = ? AND {self.SQLiteMetadata.Columns.VERSION} = ?
        """
        result = self.fetch_one(query, (package.name, str(package.version)))

        if result is None:
            logger.debug(f"Could not find '{package.name}v{package.version}' in MetadataCache")
            return None

        try:
            metadata = SingleMetadata.model_validate_json(result[0])
        except ValidationError as e:
            raise QuackPackError(e) from e

        return metadata

    def get_multi_metadata(self, package: Package) -> MultiMetadata | None:
        """
        Retrieve metadata for a specific package from the cache.
        ----
        Args:
        - `package`: The package name to retrieve metadata for.
        ----
        Returns:
        - `MultiMetadata`: The package metadata, or None if not found.
        ----
        Raises:
        - `QuackPackError`: If the metadata cannot be decoded from JSON.
        """

        logger.debug(f"Looking for '{package.name}'")
        query = f"SELECT {self.SQLiteMetadata.Columns.METADATA} FROM {self.SQLiteMetadata.TABLE} WHERE {self.SQLiteMetadata.Columns.NAME} = ?"
        results = self.fetch_all(query, (package.name,))

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
        ----
        Args:
        - `package`: The package for which metadata addition or update should be performed.
        - `metadata`: The metadata to store.
        """

        logger.debug(f"Adding '{package.name}v{package.version!s}' with {metadata=}")
        query = f"""
            INSERT OR REPLACE INTO {self.SQLiteMetadata.TABLE} (
                {self.SQLiteMetadata.Columns.NAME},
                {self.SQLiteMetadata.Columns.VERSION},
                {self.SQLiteMetadata.Columns.METADATA}
            ) VALUES (?, ?, ?)
        """

        metadata_string = metadata.model_dump_json()
        self.execute_query(query, (package.name, str(package.version), metadata_string))

    def add_multi_metadata(self, multi_package: Package, multi_metadata: MultiMetadata) -> None:
        """
        Add or update metadata for a package in multiple versions in the cache.
        ----
        Args:
        - `multi_package`: The package name for which metadata addition or update should be performed.
        - `multi_metadata`: The metadata list to store.
        """

        for metadata in multi_metadata.packages_metadata:
            package = Package(name=multi_package.name, version=str(metadata.metadata.version))

            self.add_metadata(package, metadata)


class MetadataCacheContext(SQLiteDatabaseContext[MetadataCache]):
    """
    A context manager for managing the lifecycle of a `MetadataCache` instance.

    This context manager provides an easy way to work with a `MetadataCache` instance within a `with` statement.
    It automatically handles the opening and closing of the underlying SQLite database connection.
    """

    @override
    def _create_instance(self) -> MetadataCache:
        return MetadataCache(self.db_path)
