"""
Top-level module for SQLite database and metadata cache utilities.
----
Exports:
- `MetadataCache`: A cache for storing and retrieving package metadata using an SQLite database.
- `SQLiteDatabase`: A context manager for SQLite database connections.
"""

from .metadata_cache import MetadataCache
from .sqlite_database import SQLiteDatabase

__all__ = ["MetadataCache", "SQLiteDatabase"]
