# NOTE: Trust me, I know what I am doing

# ruff: noqa: PGH004
# ruff: noqa
# type: ignore

from unittest.mock import patch

import pytest
from git import Repo
from git.exc import GitCommandError
from test_ducknest_client import ducknest_app, static_file_structure
from test_git_client import local_git_repo, mock_filepath
from test_http_client import event_loop_policy, tcp_port

from quackpack.fetcher import Fetcher
from quackpack.fetcher.api_types import MultiMetadata, Package, SingleMetadata
from quackpack.util.errors import QuackPackError


@pytest.mark.asyncio(loop_scope="class")
class TestFetcher:
    @pytest.fixture
    def metadata_cache_db_path(self, tmp_path):
        return tmp_path / "metadata_cache.db"

    async def test_get_package_metadata(self, metadata_cache_db_path, ducknest_app, tcp_port):
        with Fetcher(metadata_cache_db_path) as fetcher:
            pkg = Package(name="foo", version="1.2.3")
            result = await fetcher.get_package_metadata(f"http://localhost:{tcp_port}", pkg)

            expected = SingleMetadata.model_validate_json(
                '{ "metadata": { "author": "Patryk Rogalski", "version": "1.2.3", "id": "foo-1.2.3", "name": "foo", "license": "GLTWSPL" }, "dependencies": { "pkg2": { "version": "2.3.6" }, "pkg3": { "version": "2.4.7" } } }'
            )
            assert result == expected

            cache = fetcher.cache

            assert cache.get_metadata(pkg) == expected

    async def test_get_package_all_metadata(self, metadata_cache_db_path, ducknest_app, tcp_port):
        with Fetcher(metadata_cache_db_path) as fetcher:
            pkg = Package(name="foo")
            result = await fetcher.get_package_all_metadata(f"http://localhost:{tcp_port}", pkg)

            expected = MultiMetadata.model_validate_json(
                """{ "packages_metadata": [
                    { "metadata": { "author": "Patryk Rogalski", "version": "1.2.3", "id": "foo-1.2.3", "name": "foo", "license": "GLTWSPL" }, "dependencies": { "pkg2": { "version": "2.3.6" }, "pkg3": { "version": "2.4.7" } } },
                    { "metadata": { "author": "Patryk Rogalski", "version": "1.2.5", "id": "foo-1.2.5", "name": "foo", "license": "GLTWSPL" } }
                    ]}
                """
            )
            assert result == expected

    async def test_get_package_blob(
        self, metadata_cache_db_path, ducknest_app, tcp_port, static_file_structure
    ):
        with Fetcher(metadata_cache_db_path) as fetcher:
            pkg = Package(name="bar", version="2.5.6")
            fp = static_file_structure / "blob-bar"
            await fetcher.get_package_blob(f"http://localhost:{tcp_port}", pkg, fp)

            with open(fp, "rb") as f:
                assert f.read() == b"bar-2.5.6"

    async def test_clone(self, metadata_cache_db_path, local_git_repo, mock_filepath):
        with Fetcher(metadata_cache_db_path) as fetcher:
            url = str(local_git_repo)
            await fetcher.clone(url, mock_filepath)

            assert mock_filepath.exists()
            assert (mock_filepath / "test_file.txt").read_text() == "Hello, Git!"

    async def test_clone_with_branch(self, metadata_cache_db_path, local_git_repo, mock_filepath):
        with Fetcher(metadata_cache_db_path) as fetcher:
            url = str(local_git_repo)
            branch = "test-branch"
            await fetcher.clone(url, mock_filepath, branch=branch)

            cloned_repo = Repo(mock_filepath)
            assert cloned_repo.active_branch.name == branch

    async def test_clone_with_tag(self, metadata_cache_db_path, local_git_repo, mock_filepath):
        with Fetcher(metadata_cache_db_path) as fetcher:
            url = str(local_git_repo)
            tag = "v1.0.0"
            await fetcher.clone(url, mock_filepath, tag=tag)

            cloned_repo = Repo(mock_filepath)
            assert cloned_repo.git.describe("--tags") == tag

    async def test_clone_with_rev(self, metadata_cache_db_path, local_git_repo, mock_filepath):
        with Fetcher(metadata_cache_db_path) as fetcher:
            url = str(local_git_repo)
            repo = Repo(local_git_repo)
            rev = repo.head.commit.hexsha

            await fetcher.clone(url, mock_filepath, rev=rev)

            cloned_repo = Repo(mock_filepath)
            assert cloned_repo.head.commit.hexsha == rev

    async def test_clone_directory_exists(self, metadata_cache_db_path, local_git_repo, mock_filepath):
        with Fetcher(metadata_cache_db_path) as fetcher:
            url = str(local_git_repo)
            mock_filepath.mkdir()

            with pytest.raises(QuackPackError):
                await fetcher.clone(url, mock_filepath)

    async def test_clone_git_command_error(self, metadata_cache_db_path, local_git_repo, mock_filepath):
        with Fetcher(metadata_cache_db_path) as fetcher:
            url = str(local_git_repo)
            with patch("git.Repo.clone_from", side_effect=GitCommandError("clone failed")):
                with pytest.raises(QuackPackError):
                    await fetcher.clone(url, mock_filepath)

    async def test_clone_checkout_error(self, metadata_cache_db_path, local_git_repo, mock_filepath):
        with Fetcher(metadata_cache_db_path) as fetcher:
            url = str(local_git_repo)
            rev = "invalid-revision"

            with pytest.raises(QuackPackError):
                await fetcher.clone(url, mock_filepath, rev=rev)

    async def test_clone_branch_and_tag_error(self, metadata_cache_db_path, local_git_repo, mock_filepath):
        with Fetcher(metadata_cache_db_path) as fetcher:
            url = str(local_git_repo)
            branch = "test-branch"
            tag = "v1.0.0"

            with pytest.raises(ValueError):
                await fetcher.clone(url, mock_filepath, branch=branch, tag=tag)
