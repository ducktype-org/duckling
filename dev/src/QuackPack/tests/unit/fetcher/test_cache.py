import sqlite3
from pathlib import Path

import pytest

from quackpack.core.fetcher.api_types import Package, SingleMetadata
from quackpack.core.fetcher.cache.metadata_cache import MetadataCache, MetadataCacheContext
from quackpack.core.fetcher.cache.sqlite_database import SQLiteDatabase
from quackpack.util.types.errors import QuackPackError
from quackpack.util.types.pkgid import Identifier


@pytest.fixture(scope="function")
def temp_db_file(tmp_path: Path):
    db_path = tmp_path / "test.db"
    yield db_path


@pytest.fixture(scope="class")
def example_package():
    return Package(id=Identifier("example_package"), version="1.2.3")


@pytest.fixture(scope="class")
def example_metadata():
    content = """
{
    "metadata": {
        "authors": ["Patryk Rogalski"],
        "version": "1.2.3",
        "name": "quackpack",
        "license": "GLTWSPL",
        "description": ""
    },
    "dependencies": {
        "pkg1": {
            "version": ["2.3.4"],
            "source": {
                "inner": {
                    "type": "registry",
                    "registry_url": "xd"
                }
            },
            "features": [],
            "pinned": false,
            "conditions": {
                "system": [],
                "arch": [],
                "package_features": []
            }
        }
    },
    "dev_dependencies": {},
    "features": {},
    "targets": {},
    "profiles": {}
}
    """
    return SingleMetadata.model_validate_json(content)


class TestSQLiteDatabase:
    def test_initialize_database(self, temp_db_file: Path):
        db = SQLiteDatabase(temp_db_file)
        assert db.connection is not None
        db.close_connection()

    def test_execute_query(self, temp_db_file: Path):
        db = SQLiteDatabase(temp_db_file)
        db.execute_query("CREATE TABLE IF NOT EXISTS test_table (id INTEGER PRIMARY KEY, name TEXT)")
        db.execute_query("INSERT INTO test_table (name) VALUES (?)", ("Alice",))
        result = db.fetch_all("SELECT name FROM test_table WHERE id = 1")
        assert result == [("Alice",)]
        db.close_connection()

    def test_fetch_all(self, temp_db_file: Path):
        db = SQLiteDatabase(temp_db_file)
        db.execute_query("CREATE TABLE IF NOT EXISTS test_table (id INTEGER PRIMARY KEY, name TEXT)")
        db.execute_query("INSERT INTO test_table (name) VALUES (?)", ("Alice",))
        db.execute_query("INSERT INTO test_table (name) VALUES (?)", ("Bob",))
        result = db.fetch_all("SELECT name FROM test_table")
        assert set(result) == {("Alice",), ("Bob",)}
        db.close_connection()

    def test_fetch_one(self, temp_db_file: Path):
        db = SQLiteDatabase(temp_db_file)
        db.execute_query("CREATE TABLE IF NOT EXISTS test_table (id INTEGER PRIMARY KEY, name TEXT)")
        db.execute_query("INSERT INTO test_table (name) VALUES (?)", ("Alice",))
        result = db.fetch_one("SELECT name FROM test_table WHERE id = 1")
        assert result == ("Alice",)
        db.close_connection()

    def test_invalid_query(self, temp_db_file: Path):
        db = SQLiteDatabase(temp_db_file)
        db.execute_query("CREATE TABLE IF NOT EXISTS test_table (id INTEGER PRIMARY KEY, name TEXT)")

        with pytest.raises(sqlite3.DatabaseError):
            db.execute_query("INSERT INTO test_table (non_existing_column) VALUES (?)", ("Invalid",))

        db.close_connection()

    def test_with_rollback(self, temp_db_file: Path):
        db = SQLiteDatabase(temp_db_file)
        db.execute_query("CREATE TABLE IF NOT EXISTS test_table (id INTEGER PRIMARY KEY, name TEXT);")
        with pytest.raises(sqlite3.DatabaseError):
            db.execute_query(
                "INSERT INTO test_table (name) VALUES (?); INSERT INTO test_table (name) VALUES (?);",
                ("Alice", "Bob"),
            )
        db.close_connection(rollback=True)

        db = SQLiteDatabase(temp_db_file)
        result = db.fetch_all("SELECT name FROM test_table WHERE name = 'Alice'")
        assert result == []
        db.close_connection()


class TestMetadataCache:
    def test_initialize_metadata_cache(self, temp_db_file: Path):
        cache = MetadataCache(temp_db_file)
        assert cache.connection is not None

        table_exists = cache.fetch_all(
            f"SELECT name FROM sqlite_master WHERE type='table' AND name='{cache.SQLiteMetadata.TABLE}';"
        )
        assert table_exists == [(f"{cache.SQLiteMetadata.TABLE}",)]

        cache.close_connection()

    def test_add_and_get_metadata(
        self, temp_db_file: Path, example_package: Package, example_metadata: SingleMetadata
    ):
        cache = MetadataCache(temp_db_file)

        cache.add_metadata(example_package, example_metadata)

        retrieved_metadata = cache.get_metadata(example_package)
        assert retrieved_metadata == example_metadata

        cache.close_connection()

    def test_get_metadata_not_found(self, temp_db_file: Path, example_package: Package):
        cache = MetadataCache(temp_db_file)

        retrieved_metadata = cache.get_metadata(example_package)
        assert retrieved_metadata is None

        cache.close_connection()

    def test_get_metadata_with_invalid_json(self, temp_db_file: Path, example_package: Package):
        cache = MetadataCache(temp_db_file)
        cache.execute_query(
            f"INSERT INTO {cache.SQLiteMetadata.TABLE} ({cache.SQLiteMetadata.Columns.NAME}, {cache.SQLiteMetadata.Columns.VERSION}, {cache.SQLiteMetadata.Columns.METADATA}) VALUES (?, ?, ?)",
            (str(example_package.id), str(example_package.version), "invalid_json"),
        )

        with pytest.raises(QuackPackError):
            cache.get_metadata(example_package)

        cache.close_connection()


class TestMetadataCacheContext:
    def test_context_manager_add_and_get_metadata(
        self, temp_db_file: Path, example_package: Package, example_metadata: SingleMetadata
    ):
        with MetadataCacheContext(db_path=temp_db_file) as cache:
            assert isinstance(cache, MetadataCache)
            cache.add_metadata(example_package, example_metadata)
            retrieved_metadata = cache.get_metadata(example_package)
            assert retrieved_metadata == example_metadata
