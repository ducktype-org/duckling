"""
Top-level module for SQLite database and metadata cache utilities.
----
Exports:
- `MetadataCache`: A cache for storing and retrieving package metadata using an SQLite database.
- `MetadataCacheContext`: A context manager for managing the lifecycle of a `MetadataCache` instance.
- `SQLiteDatabase`: A wrapper for SQLite database connections.
- `SQLiteDatabaseContext`: A context manager for managing SQLite database.
"""

from .metadata_cache import MetadataCache, MetadataCacheContext
from .sqlite_database import SQLiteDatabase, SQLiteDatabaseContext

__all__ = [
    "MetadataCache",
    "MetadataCacheContext",
    "SQLiteDatabase",
    "SQLiteDatabaseContext",
]
