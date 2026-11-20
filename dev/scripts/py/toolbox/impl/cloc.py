# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

"""
`cloc` knows nothing about Duckling on its own - every `.duck`/`.dbc` file
would be skipped as an unknown extension. `duckling.cloc`, at the root of the
repository, teaches the languages to it, and this command exists so that the
definition file is never forgotten when counting.
"""

import shutil
from pathlib import Path

from .helpers import (
    bash_command,
    exit_with_error,
    log_info,
)

# impl/ -> toolbox/ -> py/ -> scripts/ -> dev/ -> repo root
_REPO_ROOT = Path(__file__).resolve().parents[5]
_LANGUAGE_DEFINITION = _REPO_ROOT / "duckling.cloc"


def cloc_impl(
    paths: tuple[str, ...] = (),
    by_file: bool = False,
    all_files: bool = False,
    cloc_path: str = "cloc",
) -> None:
    """
    Counts the lines of code with `cloc`, using Duckling's language definition.

    `paths` are resolved against the current working directory; when none are
    given, the whole repository is counted. Only the files tracked by git are
    counted, unless `all_files` is set.
    """

    if shutil.which(cloc_path) is None:
        exit_with_error(
            f"`{cloc_path}` was not found. Install cloc (e.g. `apt install cloc` or "
            "`brew install cloc`, see https://github.com/AlDanial/cloc) or point "
            "`--cloc-path` at it."
        )

    if not _LANGUAGE_DEFINITION.is_file():
        exit_with_error(
            f"The language definition `{_LANGUAGE_DEFINITION}` is missing, so Duckling "
            "sources could not be counted."
        )

    cloc_cmd = f'{cloc_path} --read-lang-def="{_LANGUAGE_DEFINITION}"'

    if by_file:
        cloc_cmd += " --by-file"

    targets = [Path(path).resolve() for path in paths]

    if all_files:
        log_info(
            "Counting untracked files as well - build directories and downloaded "
            "dependencies will be included."
        )
        cloc_cmd += "".join(f' "{target}"' for target in targets or [_REPO_ROOT])
    elif targets:
        # Counting only what git tracks keeps build directories, downloaded
        # dependencies and other generated output out of the report. The file
        # list comes from `git ls-files` rather than from cloc's own path
        # filtering, which silently reports nothing when `--vcs=git` is given
        # the path of a single file.
        pathspecs = " ".join(f"'{target}'" for target in targets)
        cloc_cmd += f' --vcs="git ls-files -- {pathspecs}"'
    else:
        cloc_cmd += " --vcs=git ."

    # The file list comes from git, so cloc has to run inside the repository.
    bash_command(cloc_cmd, cwd=_REPO_ROOT)
