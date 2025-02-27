"""
Module for caching package metadata in an SQLite database.
----
Classes:
- `MetadataCache`: A cache for storing and retrieving package metadata using an SQLite database.
"""

import json
from json import JSONDecodeError
from pathlib import Path

from common import Package, SingleMetadata
from quackpack.errors import QuackPackError
from quackpack.fetcher.cache import SQLiteDatabase


class MetadataCache(SQLiteDatabase):
    """
    A cache for storing and retrieving package metadata using an SQLite database.
    ----
    Args:
    - `db_path`: Path to the SQLite database file.
    """

    def __init__(self, db_path: Path) -> None:
        super().__init__(db_path)
        self.initialize()

    def initialize(self) -> None:
        """
        Initialize the database by creating the `packages_metadata` table if it does not exist.
        """

        # TODO: rename columns inside db
        # TODO: add constants for those names
        query = """
        CREATE TABLE IF NOT EXISTS packages_metadata (
            name TEXT NOT NULL,
            version TEXT NOT NULL,
            metadata TEXT,
            PRIMARY KEY (name, version)
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

        # TODO: rename columns inside db
        query = "SELECT value FROM packages_metadata WHERE name = ? AND version = ?"
        result = self.fetchone(query, (package.name, package.version))

        if result is None:
            return None

        try:
            metadata = json.loads(result[0])
        except JSONDecodeError as e:
            raise QuackPackError(e) from e

        return metadata

    def add_metadata(self, package: Package, metadata: SingleMetadata) -> None:
        """
        Add or update metadata for a specific package in the cache.
        ----
        Args:
        - `package`: The package to add or update metadata for.
        - `metadata`: The metadata to store.
        """

        # TODO: rename columns inside db
        query = "INSERT OR REPLACE INTO packages_metadata (name, version, metadata) VALUES (?, ?, ?)"

        metadata_string = json.dumps(metadata)
        self.execute_query(query, (package.name, package.version, metadata_string))
