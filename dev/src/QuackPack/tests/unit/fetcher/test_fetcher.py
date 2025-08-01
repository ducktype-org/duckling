# TODO: move this file into integration tests

# fixtures magic
# ruff: noqa: ARG002, F811
# pyright: reportUnknownMemberType = false, reportAttributeAccessIssue = false, reportOperatorIssue = false


from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any
from unittest.mock import patch

import pytest
from fastapi import FastAPI
from git import Repo
from git.exc import GitCommandError
from jsons import FOO123_JSON, FOO_MULTI
from test_ducknest_client import (
    ducknest_app,  # noqa: F401  # pyright: ignore[reportUnusedImport]; needed for fixtures...
    static_file_structure,  # noqa: F401  # pyright: ignore[reportUnusedImport]; needed for fixtures...
)
from test_git_client import (
    local_git_repo,  # noqa: F401  # pyright: ignore[reportUnusedImport]; needed for fixtures...
    mock_filepath,
)
from test_http_client import (
    tcp_port,  # noqa: F401  # pyright: ignore[reportUnusedImport]; needed for fixtures...
)

from quackpack.fetcher import FetcherContext
from quackpack.fetcher.api_types import MultiMetadata, Package, SingleMetadata
from quackpack.util.types.errors import QuackPackError
from quackpack.util.types.pkgid import Identifier


# Ahhhhhhh
@dataclass(frozen=False)
class GlobalContext:
    download: Path
    artifacts: Path
    metadata: Path
    console: Any

    def add_progress_bars(self) -> GlobalContext:
        return self

    def ensure_download_dir(self):
        return self.download

    def ensure_artifacts_dir(self):
        return self.artifacts

    def ensure_metadata_db(self):
        return self.metadata


@pytest.mark.asyncio(loop_scope="class")
class TestFetcher:
    # Just don't ask
    @pytest.fixture
    def ctx(self, tmp_path: Path):
        cache_path = tmp_path / "cache" / "quackpack"
        cache_path.mkdir(parents=True)

        download_cache_path = cache_path / "downloads"
        artifacts_cache_path = cache_path / "artifacts"
        metadata_cache_db_path = cache_path / "metadata_db.sqlite"

        download_cache_path.mkdir()
        artifacts_cache_path.mkdir()

        yield GlobalContext(
            download=download_cache_path, metadata=metadata_cache_db_path, artifacts=artifacts_cache_path
        )

    @pytest.mark.skip(reason="progres bars")
    async def test_get_package_metadata(self, ctx: GlobalContext, ducknest_app: FastAPI, tcp_port: int):
        with FetcherContext(ctx) as fetcher:  # pyright: ignore[reportArgumentType]; mocked GlobalContext
            pkg = Package(id=Identifier("foo"), version="1.2.3")
            result = await fetcher.get_package_metadata(f"http://localhost:{tcp_port}", pkg)

            expected = SingleMetadata.model_validate_json(FOO123_JSON)
            assert result.result == expected

            cache = fetcher.cache

            assert cache.get_metadata(pkg) == expected

    @pytest.mark.skip(reason="progres bars")
    async def test_get_package_all_metadata(self, ctx: GlobalContext, ducknest_app: FastAPI, tcp_port: int):
        with FetcherContext(ctx) as fetcher:  # pyright: ignore[reportArgumentType]; mocked GlobalContext
            pkg_name = Identifier("foo")
            result = await fetcher.get_package_all_metadata(f"http://localhost:{tcp_port}", pkg_name)

            expected = MultiMetadata.model_validate_json(FOO_MULTI)
            assert result.result == expected

    @pytest.mark.skip(reason="progres bars")
    async def test_get_package_blob(
        self, ctx: GlobalContext, ducknest_app: FastAPI, tcp_port: int, static_file_structure: Path
    ):
        with FetcherContext(ctx) as fetcher:  # pyright: ignore[reportArgumentType]; mocked GlobalContext
            pkg = Package(id=Identifier("bar"), version="2.5.6")
            fp = ctx.ensure_download_dir() / str(pkg.id) / pkg.version / fetcher.DEFAULT_BLOB_FILENAME
            result = await fetcher.get_package_blob(f"http://localhost:{tcp_port}", pkg)
            assert isinstance(result, Path) and result == fp

            with open(fp, "rb") as f:
                assert f.read() == b"bar-2.5.6"

    @pytest.mark.skip(reason="TODO: fetcher git interface changed")
    async def test_clone(self, ctx: GlobalContext, local_git_repo: Path, mock_filepaht: Path):
        with FetcherContext(ctx) as fetcher:  # pyright: ignore[reportArgumentType]; mocked GlobalContext
            url = str(local_git_repo)
            await fetcher.clone(url, mock_filepath)

            assert mock_filepath.exists()
            assert (mock_filepath / "test_file.txt").read_text() == "Hello, Git!"

    @pytest.mark.skip(reason="TODO: fetcher git interface changed")
    async def test_clone_with_branch(self, ctx: GlobalContext, local_git_repo: Path, mock_filepaht: Path):
        with FetcherContext(ctx) as fetcher:  # pyright: ignore[reportArgumentType]; mocked GlobalContext
            url = str(local_git_repo)
            branch = "test-branch"
            await fetcher.clone(url, mock_filepath, branch=branch)

            cloned_repo = Repo(mock_filepath)  # pyright: ignore[reportArgumentType]: fixtures magic
            assert cloned_repo.active_branch.name == branch

    @pytest.mark.skip(reason="TODO: fetcher git interface changed")
    async def test_clone_with_tag(self, ctx: GlobalContext, local_git_repo: Path, mock_filepaht: Path):
        with FetcherContext(ctx) as fetcher:  # pyright: ignore[reportArgumentType]; mocked GlobalContext
            url = str(local_git_repo)
            tag = "v1.0.0"
            await fetcher.clone(url, mock_filepath, tag=tag)

            cloned_repo = Repo(mock_filepath)  # pyright: ignore[reportArgumentType]: fixtures magic
            assert cloned_repo.git.describe("--tags") == tag

    @pytest.mark.skip(reason="TODO: fetcher git interface changed")
    async def test_clone_with_rev(self, ctx: GlobalContext, local_git_repo: Path, mock_filepaht: Path):
        with FetcherContext(ctx) as fetcher:  # pyright: ignore[reportArgumentType]; mocked GlobalContext
            url = str(local_git_repo)
            repo = Repo(local_git_repo)
            rev = repo.head.commit.hexsha

            await fetcher.clone(url, mock_filepath, rev=rev)

            cloned_repo = Repo(mock_filepath)  # pyright: ignore[reportArgumentType]: fixtures magic
            assert cloned_repo.head.commit.hexsha == rev

    @pytest.mark.skip(reason="TODO: fetcher git interface changed")
    async def test_clone_directory_exists(
        self, ctx: GlobalContext, local_git_repo: Path, mock_filepaht: Path
    ):
        with FetcherContext(ctx) as fetcher:  # pyright: ignore[reportArgumentType]; mocked GlobalContext
            url = str(local_git_repo)
            mock_filepath.mkdir()

            with pytest.raises(QuackPackError):
                await fetcher.clone(url, mock_filepath)

    @pytest.mark.skip(reason="TODO: fetcher git interface changed")
    async def test_clone_git_command_error(
        self, ctx: GlobalContext, local_git_repo: Path, mock_filepaht: Path
    ):
        with FetcherContext(ctx) as fetcher:  # pyright: ignore[reportArgumentType]; mocked GlobalContext
            url = str(local_git_repo)
            with (
                patch("git.Repo.clone_from", side_effect=GitCommandError("clone failed")),
                pytest.raises(QuackPackError),
            ):
                await fetcher.clone(url, mock_filepath)

    @pytest.mark.skip(reason="TODO: fetcher git interface changed")
    async def test_clone_checkout_error(self, ctx: GlobalContext, local_git_repo: Path, mock_filepaht: Path):
        with FetcherContext(ctx) as fetcher:  # pyright: ignore[reportArgumentType]; mocked GlobalContext
            url = str(local_git_repo)
            rev = "invalid-revision"

            with pytest.raises(QuackPackError):
                await fetcher.clone(url, mock_filepath, rev=rev)

    @pytest.mark.skip(reason="TODO: fetcher git interface changed")
    async def test_clone_branch_and_tag_error(
        self, ctx: GlobalContext, local_git_repo: Path, mock_filepaht: Path
    ):
        with FetcherContext(ctx) as fetcher:  # pyright: ignore[reportArgumentType]; mocked GlobalContext
            url = str(local_git_repo)
            branch = "test-branch"
            tag = "v1.0.0"

            with pytest.raises(ValueError):
                await fetcher.clone(url, mock_filepath, branch=branch, tag=tag)
