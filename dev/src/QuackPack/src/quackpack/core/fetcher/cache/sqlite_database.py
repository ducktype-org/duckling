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

    Provides methods to execute SQL queries and retrieve results using a persistent connection.

    :param pathlib.Path db_path: Path to the SQLite database file.
    """

    def __init__(self, db_path: Path) -> None:
        logger.debug(f"Initializing SQLite database at '{db_path}'")
        self.connection = sqlite3.connect(db_path)

    def close_connection(self, rollback: bool = False) -> None:
        """
        Close the SQLite connection, optionally rolling back the current transaction.

        :param bool rollback: Whether to roll back before closing the connection.
        """

        if rollback:
            self.connection.rollback()
        self.connection.close()

    def execute_query(self, query: str, params: tuple[Any, ...] = ()) -> None:
        """
        Execute an SQL query with optional parameters.

        :param str query: The SQL query to execute.
        :param tuple[Any, ...] params: Tuple of parameters to substitute into the SQL query.
        """

        logger.debug(f"Executing SQLite query '{query}' with {params=}")

        cursor = self.connection.cursor()
        cursor.execute(query, params)
        self.connection.commit()

    def fetch_all(self, query: str, params: tuple[Any, ...] = ()) -> list[tuple[Any, ...]]:
        """
        Execute an SQL query and fetch all results.

        :param str query: The SQL query to execute.
        :param tuple[Any, ...] params: Tuple of parameters to substitute into the SQL query.
        :return: A list of result rows, each as a tuple.
        :rtype: list[tuple[Any, ...]]
        """

        logger.debug(f"Fetching all SQLite queries '{query}' with {params=}")

        cursor = self.connection.cursor()
        cursor.execute(query, params)
        self.connection.commit()
        return cursor.fetchall()

    def fetch_one(self, query: str, params: tuple[Any, ...] = ()) -> tuple[Any, ...] | None:
        """
        Execute an SQL query and fetch a single result.

        :param str query: The SQL query to execute.
        :param tuple[Any, ...] params: Tuple of parameters to substitute into the SQL query.
        :return: A single result row as a tuple, or None if no result is found.
        :rtype: tuple[Any, ...] | None
        """

        logger.debug(f"Fetching all SQLite query '{query}' with {params=}")

        cursor = self.connection.cursor()
        cursor.execute(query, params)
        self.connection.commit()
        return cursor.fetchone()


class SQLiteDatabaseContext[T: SQLiteDatabase](AbstractContextManager[T], ABC):
    """
    A context manager for handling the lifecycle of an SQLite database connection.

    Intended to be used with a ``with`` statement. It opens the connection on entry
    and closes it on exit, rolling back if an exception occurred.

    :param pathlib.Path db_path: Path to the SQLite database file.
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
        Create and return a new instance of the SQLite database wrapper.

        This must be implemented by subclasses.

        :return: An instance of the SQLite database.
        :rtype: T
        """
