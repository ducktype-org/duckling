"""
Module for interacting with SQLite databases.
----
Classes:
- `SQLiteDatabase`: A wrapper for SQLite database connections, providing methods for executing queries and fetching results.
- `SQLiteDatabaseContext`: A context manager for managing SQLite database connections in a `with` statement.
"""

import sqlite3
from abc import ABC, abstractmethod
from contextlib import AbstractContextManager
from pathlib import Path
from types import TracebackType
from typing import Any, override

from quackpack.util.logger import get_logger

logger = get_logger(__name__)


class SQLiteDatabase:
    """
    A context manager for SQLite database connections.
    Provides methods to execute queries and fetch results.
    ----
    Args:
    - `db_path`: Path to the SQLite database file.
    """

    def __init__(self, db_path: Path) -> None:
        logger.debug(f"Initializing SQLite database at '{db_path}'")
        self.connection = sqlite3.connect(db_path)

    def close_connection(self, rollback: bool = False) -> None:
        """
        Closes the SQLite connection, optionally rolling back any uncommitted transactions.
        ----
        Args:
        - `rollback`: Whether to roll back the current transaction before closing. Defaults to False.
        """

        if rollback:
            self.connection.rollback()
        self.connection.close()

    def execute_query(self, query: str, params: tuple[Any, ...] = ()) -> None:
        """
        Execute an SQL query with optional parameters.
        ----
        Args:
        - `query`: The SQL query to execute.
        - `params`: Optional tuple of parameters for the query.
        """

        logger.debug(f"Executing SQLite query '{query}' with {params=}")

        cursor = self.connection.cursor()
        cursor.execute(query, params)
        self.connection.commit()

    def fetch_all(self, query: str, params: tuple[Any, ...] = ()) -> list[tuple[Any, ...]]:
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

        logger.debug(f"Fetching all SQLite queries '{query}' with {params=}")

        cursor = self.connection.cursor()
        cursor.execute(query, params)
        self.connection.commit()
        return cursor.fetchall()

    def fetch_one(self, query: str, params: tuple[Any, ...] = ()) -> tuple[Any, ...] | None:
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

        logger.debug(f"Fetching all SQLite query '{query}' with {params=}")

        cursor = self.connection.cursor()
        cursor.execute(query, params)
        self.connection.commit()
        return cursor.fetchone()


class SQLiteDatabaseContext[T: SQLiteDatabase](AbstractContextManager[T], ABC):
    """
    A context manager for SQLite database connections used in `with` statements.
    Manages the lifetime of an SQLite connection, ensuring it is opened and closed appropriately.
    ----
    Args:
    - `db_path`: Path to the SQLite database file.
    """

    def __init__(self, db_path: Path) -> None:
        self.db_path = db_path
        self.sqlite_database: T | None = None

    @override
    def __enter__(self) -> T:
        logger.debug(f"Connecting to SQLite at '{self.db_path}'")
        self.sqlite_database = self._create_instance()
        return self.sqlite_database

    @override
    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_value: BaseException | None,
        traceback: TracebackType | None,
    ) -> bool | None:
        logger.debug(f"Closing connection to SQLite at '{self.db_path}'")
        if self.sqlite_database is not None:
            self.sqlite_database.close_connection(exc_type is not None)

        return False if exc_type is not None else None

    @abstractmethod
    def _create_instance(self) -> T:
        """
        Creates an instance of `SQLiteDatabase`.

        This method must be implemented by subclasses to create an instance of the appropriate database class.
        ----
        Returns:
        - `SQLiteDatabase`: An instance of the SQLite database.
        """
