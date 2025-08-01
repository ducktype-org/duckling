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

from quackpack.fetcher.api_types import SingleMetadata
from quackpack.fetcher.util.git_progress import GitRemoteProgress
from quackpack.global_context import GlobalContext
from quackpack.package_loader import PackageLoader
from quackpack.util.logger import get_logger
from quackpack.util.types.errors import QuackPackError

logger = get_logger(__name__)


class GitClient:
    """
    A client for interacting with Git repositories.
    """

    @classmethod
    async def clone(
        cls,
        url: str,
        destination: Path,
        ctx: GlobalContext,
        *,
        branch: str | None = None,
        tag: str | None = None,
        rev: str | None = None,
        progress: GitRemoteProgress | None = None,
    ) -> tuple[str, SingleMetadata]:
        """
        Clone a Git repository to a local directory and fetch package metadata.

        :param str url: The URL of the Git repository to clone.
        :param pathlib.Path destination: The local path where the repository will be cloned.
        :param str | None branch: The branch to clone.
        :param str | None tag: The tag to clone.
        :param str | None rev: The revision (commit hash) to checkout after cloning.
        :raises quackpack.util.errors.QuackPackError: If the destination exists or a Git command fails.
        :raises ValueError: If both ``branch`` and ``tag`` are provided simultaneously.
        :return: The fetched package Git commit hash and configuration metadata.
        :rtype: tuple[str, quackpack.fetcher.api_types.SingleMetadata]
        """

        logger.debug(f"Git client: cloning '{url}' to {destination}; {branch=}, {tag=}, {rev=}")

        if branch is not None and tag is not None:
            raise ValueError("Expected at most one option of 'branch' and 'tag'.")

        clone_options: dict[str, Any] = {}

        if rev is None:
            clone_options["depth"] = 1

        if branch is not None or tag is not None:
            clone_options["branch"] = branch or tag

        try:
            if progress is not None:
                repo = Repo.clone_from(url, destination, **clone_options, progress=progress)  # pyright: ignore[reportArgumentType]
            else:
                repo = Repo.clone_from(url, destination, **clone_options)
        except (GitCommandNotFound, GitCommandError) as e:
            raise QuackPackError(e) from e

        if rev is not None:
            try:
                repo.git.checkout(rev)
            except GitCommandError as e:
                raise QuackPackError(e) from e

        metadata = PackageLoader.find_at_exact_directory(destination, ctx).manifest
        commit_hash = str(repo.rev_parse("HEAD"))

        return commit_hash, metadata.summary.into_schema()
