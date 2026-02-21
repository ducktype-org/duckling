from dataclasses import dataclass
from pathlib import Path
from unittest.mock import patch

import pytest
from git import Repo
from git.exc import GitCommandError

from quackpack.core.fetcher.client.git_client import GitClient
from quackpack.core.package_loader import PackageLoader
from quackpack.util.types.errors import QuackPackError


# Ahhhhhhh
@dataclass(frozen=False)
class GlobalContext:
    def registry_url(self) -> str:
        return "xd"


@pytest.fixture
def local_git_repo(tmp_path: Path):
    """Fixture to create a local Git repository for testing."""
    repo_dir = tmp_path / "local_repo"
    repo_dir.mkdir()
    print(type(tmp_path))

    repo = Repo.init(repo_dir)

    (repo_dir / "test_file.txt").write_text("Hello, Git!")
    (repo_dir / PackageLoader.MANIFEST_NAME).write_text("""
    metadata:
        name: fixtured_git_dependency
        version: 1.0""")
    repo.index.add(
        ["test_file.txt", PackageLoader.MANIFEST_NAME]
    )  # pyright: ignore[reportUnknownMemberType]; `add()` has generic lambda inside.
    repo.index.commit("Initial commit")

    repo.create_head("test-branch")
    repo.create_tag("v1.0.0")

    return repo_dir


@pytest.fixture
def mock_filepath(tmp_path: Path):
    """Fixture to provide a temporary directory for cloning."""
    return tmp_path / "cloned_repo"


@pytest.mark.asyncio
class TestGitClient:
    async def test_clone_local_repo(self, local_git_repo: Path, mock_filepath: Path):
        url = str(local_git_repo)
        await GitClient.clone(url, mock_filepath, ctx=GlobalContext())

        assert mock_filepath.exists()

        _cloned_repo = Repo(mock_filepath)
        assert (mock_filepath / "test_file.txt").read_text() == "Hello, Git!"

    async def test_clone_local_repo_with_branch(
        self, local_git_repo: Path, mock_filepath: Path
    ):
        url = str(local_git_repo)
        branch = "test-branch"
        await GitClient.clone(url, mock_filepath, branch=branch, ctx=GlobalContext())

        cloned_repo = Repo(mock_filepath)
        assert cloned_repo.active_branch.name == branch

    async def test_clone_local_repo_with_tag(
        self, local_git_repo: Path, mock_filepath: Path
    ):
        url = str(local_git_repo)
        tag = "v1.0.0"
        await GitClient.clone(url, mock_filepath, tag=tag, ctx=GlobalContext())

        cloned_repo = Repo(mock_filepath)
        assert cloned_repo.git.describe("--tags") == tag

    async def test_clone_local_repo_with_rev(
        self, local_git_repo: Path, mock_filepath: Path
    ):
        url = str(local_git_repo)
        repo = Repo(local_git_repo)
        rev = repo.head.commit.hexsha

        await GitClient.clone(url, mock_filepath, rev=rev, ctx=GlobalContext())

        cloned_repo = Repo(mock_filepath)
        assert cloned_repo.head.commit.hexsha == rev

    async def test_clone_local_repo_git_command_error(
        self, local_git_repo: Path, mock_filepath: Path
    ):
        url = str(local_git_repo)
        with (
            patch("git.Repo.clone_from", side_effect=GitCommandError("clone failed")),
            pytest.raises(QuackPackError),
        ):
            await GitClient.clone(url, mock_filepath, ctx=GlobalContext())

    async def test_clone_local_repo_checkout_error(
        self, local_git_repo: Path, mock_filepath: Path
    ):
        url = str(local_git_repo)
        rev = "invalid-revision"

        with pytest.raises(QuackPackError):
            await GitClient.clone(url, mock_filepath, rev=rev, ctx=GlobalContext())

    async def test_clone_local_repo_branch_and_tag_error(
        self, local_git_repo: Path, mock_filepath: Path
    ):
        url = str(local_git_repo)
        branch = "test-branch"
        tag = "v1.0.0"

        with pytest.raises(ValueError):
            await GitClient.clone(
                url, mock_filepath, branch=branch, tag=tag, ctx=GlobalContext()
            )
