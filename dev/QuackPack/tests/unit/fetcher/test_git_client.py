# NOTE: Trust me, I know what I am doing

# ruff: noqa: PGH004
# ruff: noqa
# type: ignore

from unittest.mock import patch

import pytest
from git import Repo
from git.exc import GitCommandError

from quackpack.fetcher.client.git_client import GitClient
from quackpack.util.errors import QuackPackError


@pytest.fixture
def local_git_repo(tmp_path):
    """Fixture to create a local Git repository for testing."""
    repo_dir = tmp_path / "local_repo"
    repo_dir.mkdir()

    repo = Repo.init(repo_dir)

    (repo_dir / "test_file.txt").write_text("Hello, Git!")
    repo.index.add(["test_file.txt"])
    repo.index.commit("Initial commit")

    repo.create_head("test-branch")
    repo.create_tag("v1.0.0")

    return repo_dir


@pytest.fixture
def mock_filepath(tmp_path):
    """Fixture to provide a temporary directory for cloning."""
    return tmp_path / "cloned_repo"


@pytest.mark.asyncio
class TestGitClient:
    async def test_clone_local_repo(self, local_git_repo, mock_filepath):
        url = str(local_git_repo)
        await GitClient.clone(url, mock_filepath)

        assert mock_filepath.exists()

        cloned_repo = Repo(mock_filepath)
        assert (mock_filepath / "test_file.txt").read_text() == "Hello, Git!"

    async def test_clone_local_repo_with_branch(self, local_git_repo, mock_filepath):
        url = str(local_git_repo)
        branch = "test-branch"
        await GitClient.clone(url, mock_filepath, branch=branch)

        cloned_repo = Repo(mock_filepath)
        assert cloned_repo.active_branch.name == branch

    async def test_clone_local_repo_with_tag(self, local_git_repo, mock_filepath):
        url = str(local_git_repo)
        tag = "v1.0.0"
        await GitClient.clone(url, mock_filepath, tag=tag)

        cloned_repo = Repo(mock_filepath)
        assert cloned_repo.git.describe("--tags") == tag

    async def test_clone_local_repo_with_rev(self, local_git_repo, mock_filepath):
        url = str(local_git_repo)
        repo = Repo(local_git_repo)
        rev = repo.head.commit.hexsha

        await GitClient.clone(url, mock_filepath, rev=rev)

        cloned_repo = Repo(mock_filepath)
        assert cloned_repo.head.commit.hexsha == rev

    async def test_clone_local_repo_directory_exists(self, local_git_repo, mock_filepath):
        url = str(local_git_repo)
        mock_filepath.mkdir()

        with pytest.raises(QuackPackError):
            await GitClient.clone(url, mock_filepath)

    async def test_clone_local_repo_git_command_error(self, local_git_repo, mock_filepath):
        url = str(local_git_repo)
        with patch("git.Repo.clone_from", side_effect=GitCommandError("clone failed")):
            with pytest.raises(QuackPackError):
                await GitClient.clone(url, mock_filepath)

    async def test_clone_local_repo_checkout_error(self, local_git_repo, mock_filepath):
        url = str(local_git_repo)
        rev = "invalid-revision"

        with pytest.raises(QuackPackError):
            await GitClient.clone(url, mock_filepath, rev=rev)

    async def test_clone_local_repo_branch_and_tag_error(self, local_git_repo, mock_filepath):
        url = str(local_git_repo)
        branch = "test-branch"
        tag = "v1.0.0"

        with pytest.raises(ValueError):
            await GitClient.clone(url, mock_filepath, branch=branch, tag=tag)
