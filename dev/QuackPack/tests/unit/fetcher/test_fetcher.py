# REMARK: https://xkcd.com/963/
# NOTE: Trust me, I know what I am doing

# ruff: noqa: PGH004
# ruff: noqa
# type: ignore

# TODO: move this file into integration tests

from unittest.mock import patch

import pytest
from dataclasses import dataclass
from pathlib import Path
from git import Repo
from git.exc import GitCommandError

from quackpack.fetcher import Fetcher
from quackpack.fetcher.api_types import MultiMetadata, Package, SingleMetadata
from quackpack.util.errors import QuackPackError
from test_ducknest_client import ducknest_app, static_file_structure
from test_git_client import local_git_repo, mock_filepath
from test_http_client import tcp_port
from quackpack.util.pkgid import Identifier


# Ahhhhhhh
@dataclass(frozen=False)
class _Cache:
    download_dir: Path
    metadata_db_path: Path
    artifacts_dir: Path


# Ahhhhhhh
@dataclass(frozen=False)
class _Config:
    cache: _Cache


# Ahhhhhhh
@dataclass(frozen=False)
class GlobalContext:
    configuration: _Config


@pytest.mark.asyncio(loop_scope="class")
class TestFetcher:
    # Just don't ask
    @pytest.fixture
    def ctx(self, tmp_path):
        cache_path = tmp_path / "cache" / "quackpack"
        cache_path.mkdir(parents=True)

        download_cache_path = cache_path / "downloads"
        artifacts_cache_path = cache_path / "artifacts"
        metadata_cache_db_path = cache_path / "metadata_db.sqlite"

        download_cache_path.mkdir()
        artifacts_cache_path.mkdir()

        yield GlobalContext(
            configuration=_Config(
                cache=_Cache(
                    download_dir=download_cache_path,
                    metadata_db_path=metadata_cache_db_path,
                    artifacts_dir=artifacts_cache_path,
                )
            )
        )

    async def test_get_package_metadata(self, ctx, ducknest_app, tcp_port):
        with Fetcher(ctx) as fetcher:
            pkg = Package(id="foo", version="1.2.3")
            result = await fetcher.get_package_metadata(f"http://localhost:{tcp_port}", pkg)

            expected = SingleMetadata.model_validate_json(
                '{ "metadata": { "author": "Patryk Rogalski", "version": "1.2.3", "name": "foo" }, "dependencies": { "pkg2": { "version": "2.3.6" }, "pkg3": { "version": "2.4.7" } } }'
            )
            assert result.result == expected

            cache = fetcher.cache

            assert cache.get_metadata(pkg) == expected

    async def test_get_package_all_metadata(self, ctx, ducknest_app, tcp_port):
        with Fetcher(ctx) as fetcher:
            pkg_name = "foo"
            result = await fetcher.get_package_all_metadata(f"http://localhost:{tcp_port}", pkg_name)

            expected = MultiMetadata.model_validate_json(
                """{ "packages_metadata": [
                    { "metadata": { "author": "Patryk Rogalski", "version": "1.2.3", "name": "foo" }, "dependencies": { "pkg2": { "version": "2.3.6" }, "pkg3": { "version": "2.4.7" } } },
                    { "metadata": { "author": "Patryk Rogalski", "version": "1.2.5", "name": "foo" } }
                    ]}
                """
            )
            assert result.result == expected

    async def test_get_package_blob(self, ctx, ducknest_app, tcp_port, static_file_structure):
        with Fetcher(ctx) as fetcher:
            pkg = Package(id="bar", version="2.5.6")
            fp = (
                ctx.configuration.cache.download_dir
                / str(pkg.id)
                / pkg.version
                / fetcher.DEFAULT_BLOB_FILENAME
            )
            result = await fetcher.get_package_blob(f"http://localhost:{tcp_port}", pkg)
            assert isinstance(result, Path) and result == fp

            with open(fp, "rb") as f:
                assert f.read() == b"bar-2.5.6"

    @pytest.mark.skip(reason="TODO: fetcher git interface changed")
    async def test_clone(self, ctx, local_git_repo, mock_filepath):
        with Fetcher(ctx) as fetcher:
            url = str(local_git_repo)
            await fetcher.clone(url, mock_filepath)

            assert mock_filepath.exists()
            assert (mock_filepath / "test_file.txt").read_text() == "Hello, Git!"

    @pytest.mark.skip(reason="TODO: fetcher git interface changed")
    async def test_clone_with_branch(self, ctx, local_git_repo, mock_filepath):
        with Fetcher(ctx) as fetcher:
            url = str(local_git_repo)
            branch = "test-branch"
            await fetcher.clone(url, mock_filepath, branch=branch)

            cloned_repo = Repo(mock_filepath)
            assert cloned_repo.active_branch.name == branch

    @pytest.mark.skip(reason="TODO: fetcher git interface changed")
    async def test_clone_with_tag(self, ctx, local_git_repo, mock_filepath):
        with Fetcher(ctx) as fetcher:
            url = str(local_git_repo)
            tag = "v1.0.0"
            await fetcher.clone(url, mock_filepath, tag=tag)

            cloned_repo = Repo(mock_filepath)
            assert cloned_repo.git.describe("--tags") == tag

    @pytest.mark.skip(reason="TODO: fetcher git interface changed")
    async def test_clone_with_rev(self, ctx, local_git_repo, mock_filepath):
        with Fetcher(ctx) as fetcher:
            url = str(local_git_repo)
            repo = Repo(local_git_repo)
            rev = repo.head.commit.hexsha

            await fetcher.clone(url, mock_filepath, rev=rev)

            cloned_repo = Repo(mock_filepath)
            assert cloned_repo.head.commit.hexsha == rev

    @pytest.mark.skip(reason="TODO: fetcher git interface changed")
    async def test_clone_directory_exists(self, ctx, local_git_repo, mock_filepath):
        with Fetcher(ctx) as fetcher:
            url = str(local_git_repo)
            mock_filepath.mkdir()

            with pytest.raises(QuackPackError):
                await fetcher.clone(url, mock_filepath)

    @pytest.mark.skip(reason="TODO: fetcher git interface changed")
    async def test_clone_git_command_error(self, ctx, local_git_repo, mock_filepath):
        with Fetcher(ctx) as fetcher:
            url = str(local_git_repo)
            with patch("git.Repo.clone_from", side_effect=GitCommandError("clone failed")):
                with pytest.raises(QuackPackError):
                    await fetcher.clone(url, mock_filepath)

    @pytest.mark.skip(reason="TODO: fetcher git interface changed")
    async def test_clone_checkout_error(self, ctx, local_git_repo, mock_filepath):
        with Fetcher(ctx) as fetcher:
            url = str(local_git_repo)
            rev = "invalid-revision"

            with pytest.raises(QuackPackError):
                await fetcher.clone(url, mock_filepath, rev=rev)

    @pytest.mark.skip(reason="TODO: fetcher git interface changed")
    async def test_clone_branch_and_tag_error(self, ctx, local_git_repo, mock_filepath):
        with Fetcher(ctx) as fetcher:
            url = str(local_git_repo)
            branch = "test-branch"
            tag = "v1.0.0"

            with pytest.raises(ValueError):
                await fetcher.clone(url, mock_filepath, branch=branch, tag=tag)
