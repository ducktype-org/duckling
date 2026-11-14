# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

import os
import shlex
import shutil
import subprocess

from .helpers import (
    get_input,
    log_good,
    log_info,
    log_new_line,
    log_warning,
)
from .list_files import list_files_impl

DUCKLING_EXTENSIONS = [".dk"]

# Exit codes of `duckfmt -k`, any other one means duckfmt failed on the file
_DUCKFMT_OK = 0
_DUCKFMT_NOT_FORMATTED = 1


def _resolve_duckfmt(build_dir: str, duckfmt_path: str | None) -> str | None:
    """
    The duckfmt binary to run: `duckfmt_path` when given (a path or a name on the
    PATH), otherwise `<build_dir>/bin/duckfmt`. None when it cannot be found.
    """
    candidate = duckfmt_path or os.path.join(build_dir, "bin", "duckfmt")
    if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
        # Absolute, so the hint that names it works from any directory
        return os.path.abspath(candidate)
    return shutil.which(candidate)


def _check_file(duckfmt: str, file: str) -> bool | None:
    """
    True when the file is formatted, False when it is not, None when duckfmt failed on it.
    """
    returncode = subprocess.run([duckfmt, file, "-k"]).returncode
    if returncode == _DUCKFMT_OK:
        return True
    if returncode == _DUCKFMT_NOT_FORMATTED:
        return False
    return None


def _format_in_place(duckfmt: str, file: str) -> None:
    subprocess.run([duckfmt, file, "-i"])


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

    results = {f: _check_file(duckfmt, f) for f in files}
    # duckfmt cannot format these, so they are left out of the fix
    failed = [f for f, formatted in results.items() if formatted is None]
    unformatted = [f for f, formatted in results.items() if formatted is False]

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
            rechecked = {f: _check_file(duckfmt, f) for f in unformatted}
            failed += [f for f, formatted in rechecked.items() if formatted is None]
            unformatted = [
                f for f, formatted in rechecked.items() if formatted is False
            ]
            if not unformatted:
                log_good("Duckling files formatted!")

    if failed:
        log_warning(
            f"duckfmt failed on {len(failed)} Duckling file(s), see the errors above: "
            f"[{', '.join(failed)}]"
        )

    if unformatted:
        fix_command = (
            "./scripts/formatting/format_repo_duckling.sh"
            f"{' --all' if all else ''} {shlex.quote(duckfmt)}"
        )
        log_warning(
            f"{len(unformatted)} Duckling file(s) are not formatted. "
            f"Run `{fix_command}` to format them."
        )
        if not all and (no_merge_base or branch != "origin/main"):
            log_warning(
                "The script always diffs against the merge base with origin/main, so it "
                f"can format a different set of files than the one checked against `{branch}`."
            )

    if failed or unformatted:
        return False

    log_good(f"All {len(files)} checked Duckling file(s) are formatted")
    return True
