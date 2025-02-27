"""
Module for interacting with Git repositories.
----
Classes:
- `GitClient`: A client for cloning Git repositories and managing Git operations.
"""

from pathlib import Path
from typing import Any

from git import Repo
from git.exc import GitCommandError, GitCommandNotFound

from quackpack.errors import QuackPackError


class GitClient:
    """
    A client for interacting with Git repositories.
    """

    @classmethod
    async def clone(
        cls,
        url: str,
        filepath: Path,
        *,
        branch: str | None = None,
        tag: str | None = None,
        rev: str | None = None,
    ) -> None:
        """
        Clone a Git repository to the specified filepath.
        ----
        Args:
        - `url`: The URL of the Git repository.
        - `filepath`: The local path where the repository will be cloned.
        - `branch`: Optional branch to clone.
        - `tag`: Optional tag to clone.
        - `rev`: Optional revision (commit hash) to checkout after cloning.
        ----
        Raises:
        - `QuackPackError`: If the directory already exists or if a Git command fails.
        - `ValueError`: If both `branch` and `tag` are provided.
        """

        try:
            filepath.mkdir(parents=True, exist_ok=False)
        except FileExistsError as e:
            raise QuackPackError(e) from e

        if branch is not None and tag is not None:
            raise ValueError("Expected at most one option of 'branch' and 'tag'.")

        clone_options: dict[str, Any] = {}

        if rev is None:
            clone_options["depth"] = 1

        if branch is not None or tag is not None:
            clone_options["branch"] = branch or tag

        # TODO: add progress bar
        try:
            repo = Repo.clone_from(url, filepath, **clone_options)
        except (GitCommandNotFound, GitCommandError) as e:
            raise QuackPackError(e) from e

        if rev is not None:
            try:
                repo.git.checkout(rev)
            except GitCommandError as e:
                raise QuackPackError(e) from e
