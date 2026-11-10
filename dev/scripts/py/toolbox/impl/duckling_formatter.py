# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

import os
import shutil
import subprocess
import sys

from .helpers import (
    get_input,
    log_good,
    log_info,
    log_new_line,
    log_warning,
)
from .list_files import list_files_impl

DUCKLING_EXTENSIONS = [".dk"]


def _resolve_duckfmt(build_dir: str, duckfmt_path: str | None) -> str | None:
    """
    The duckfmt binary to run: `duckfmt_path` when given (a path or a name on the
    PATH), otherwise `<build_dir>/bin/duckfmt`. None when it cannot be found.
    """
    candidate = duckfmt_path or os.path.join(build_dir, "bin", "duckfmt")
    if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
        return candidate
    return shutil.which(candidate)


def _is_formatted(duckfmt: str, file: str) -> bool:
    """`duckfmt -k` exits non-zero and names the file on stderr when it is not formatted."""
    return subprocess.run([duckfmt, file, "-k"]).returncode == 0


def _format_in_place(duckfmt: str, file: str) -> bool:
    return subprocess.run([duckfmt, file, "-i"]).returncode == 0


def duckling_formatter_impl(
    all: bool = False,
    branch: str = "origin/main",
    no_merge_base: bool = False,
    build_dir: str = "build",
    duckfmt_path: str | None = None,
    no_fix: bool = False,
    auto_fix: bool = False,
) -> bool:
    """
    Check that Duckling sources are formatted with duckfmt.
    Returns True if every checked file is formatted (or was fixed), False otherwise.

    Parameters:
    - all: If True, check every Duckling file. If False, only the ones changed
      against `branch`, plus the untracked ones.
    - branch: The branch to compare against when checking changed files.
    - no_merge_base: If True, compare against `branch` itself, not the merge base.
    - build_dir: The build directory holding `bin/duckfmt`.
    - duckfmt_path: An explicit duckfmt binary; overrides `build_dir`.
    - no_fix: If True, only report the unformatted files.
    - auto_fix: If True, format the unformatted files without prompting.
    """
    duckfmt = _resolve_duckfmt(build_dir, duckfmt_path)
    if duckfmt is None:
        log_warning(
            f"duckfmt was not found (looked for `{duckfmt_path or build_dir + '/bin/duckfmt'}`). "
            f"Build it first: `cmake --build {build_dir} --target duckfmt`."
        )
        return False

    # Same scope as `scripts/formatting/format_repo_duckling.sh`, so the fix it
    # suggests covers exactly what was checked here.
    files = list_files_impl(
        extensions=DUCKLING_EXTENSIONS,
        branch=branch,
        only_modified=not all,
        no_merge_base=no_merge_base,
        include_untracked=True,
        filter_deleted=True,
    )
    if not files:
        log_info("No Duckling files to check. Nothing to check.")
        return True

    unformatted = [f for f in files if not _is_formatted(duckfmt, f)]

    if unformatted and not no_fix:
        apply = auto_fix
        if not auto_fix:
            log_new_line()
            log_info(f"{len(unformatted)} Duckling file(s) are not formatted.")
            response = get_input("Do you want to format them now? [Y/n]")
            apply = response.lower() in ["y", "yes", ""]

        if apply:
            for f in unformatted:
                _format_in_place(duckfmt, f)
            unformatted = [f for f in unformatted if not _is_formatted(duckfmt, f)]
            if not unformatted:
                log_good("Duckling files formatted!")

    if unformatted:
        log_warning(
            f"{len(unformatted)} Duckling file(s) are not formatted. Run "
            f"`./scripts/formatting/format_repo_duckling.sh{' --all' if all else ''}` "
            "to format them."
        )
        return False

    log_good(f"All {len(files)} checked Duckling file(s) are formatted")
    return True
