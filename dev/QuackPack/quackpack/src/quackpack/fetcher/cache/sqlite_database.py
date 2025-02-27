"""
Module for interacting with SQLite databases.
----
Classes:
- `SQLiteDatabase`: A context manager for SQLite database connections, providing methods for executing queries and fetching results.
"""

import sqlite3
from pathlib import Path
from types import TracebackType
from typing import Any, Self


class SQLiteDatabase:
    """
    A context manager for SQLite database connections.
    ----
    Args:
    - `db_path`: Path to the SQLite database file.
    """

    def __init__(self, db_path: Path) -> None:
        self.db_path: Path = db_path
        self.connection: sqlite3.Connection | None = None

    def __enter__(self) -> Self:
        self.connection = sqlite3.connect(self.db_path)
        return self

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: TracebackType | None,
    ) -> bool | None:
        if self.connection is None:
            return

        if exc_type is not None:
            self.connection.rollback()
        self.connection.close()

        if exc_type is not None:
            return False

    def execute_query(self, query: str, params: tuple[Any, ...] = ()) -> None:
        """
        Execute an SQL query with optional parameters.
        ----
        Args:
        - `query`: The SQL query to execute.
        - `params`: Optional tuple of parameters for the query.
        """

        if self.connection is None:
            return

        cursor = self.connection.cursor()
        cursor.execute(query, params)
        self.connection.commit()

    def fetchall(self, query: str, params: tuple[Any, ...] = ()) -> list[tuple[Any, ...]]:
        """
        Execute an SQL query and fetch all results.
        ----
        Args:
        - `query`: The SQL query to execute.
        - `params`: Optional tuple of parameters for the query.
        ----
        Returns:
        - `list[tuple[Any, ...]]`: A list of tuples representing the query results.
        """

        if self.connection is None:
            return []

        cursor = self.connection.cursor()
        cursor.execute(query, params)
        self.connection.commit()
        return cursor.fetchall()

    def fetchone(self, query: str, params: tuple[Any, ...] = ()) -> tuple[Any, ...] | None:
        """
        Execute an SQL query and fetch a single result.
        ----
        Args:
        - `query`: The SQL query to execute.
        - `params`: Optional tuple of parameters for the query.
        ----
        Returns:
        - `tuple[Any, ...] | None`: A tuple representing the query result, or None if no result is found.
        """

        if self.connection is None:
            return None

        cursor = self.connection.cursor()
        cursor.execute(query, params)
        self.connection.commit()
        return cursor.fetchone()
