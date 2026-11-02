# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

from typing import Callable, List
import re
from pathlib import Path

from .cpp_linter import get_files_for_linter
from .helpers import (
    log_good,
    log_info,
    log_warning,
    log_new_line,
    get_input,
)

_RELATIVE_IMPORT_REGEX = re.compile(r'#include "(.*?)"')


def duck_linter_impl(
    all: bool = False,
    branch: str = "origin/main",
    verbose: bool = False,
    no_merge_base: bool = False,
    no_fix: bool = False,
    auto_fix: bool = False,
) -> bool:
    """
    Main implementation of the duck linter.
    Returns True if all files passed the linter, False otherwise.

    Parameters:
    - all: If True, lint all source files. If False, lint only changed files.
    - branch: The branch to compare against when linting changed files.
    - verbose: If True, print detailed output for each file.
    - no_merge_base: If True, do not use the merge base for determining changed files.
    - no_fix: If True, do not apply automatic fixes.
    - auto_fix: If True, apply automatic fixes.
    """

    failed_files: set[SourceFile] = set(
        filter(
            lambda f: not f.verify(verbose),
            get_source_files(all, branch, no_merge_base),
        )
    )

    if failed_files and not no_fix:
        apply = auto_fix
        if not auto_fix:
            total_fixes = sum(f.getFixCount() for f in failed_files)
            log_new_line()
            log_info(
                f"Can perform {total_fixes} automatic fix(es) across {len(failed_files)} file(s)."
            )
            response = get_input("Do you want to apply these fixes? [Y/n]")
            apply = response.lower() in ["y", "yes", ""]

        if apply:
            for f in failed_files:
                f.applyFixes()
            log_good("Fixes applied!")

            fixed_files: set[SourceFile] = set()
            for f in failed_files:
                # This file was fixed, reload and re-check
                f.reloadAndReset()
                if f.verify(False):
                    fixed_files.add(f)
            failed_files.difference_update(fixed_files)

    if failed_files:
        return False

    log_good("All checked files passed the duck linter")
    return True


class SourceFile:
    def __init__(self, path: str):
        self.path = Path(path)
        self.dir = self.path.parent

        self.errors: List[str] = []
        self.fixes: List[Callable[[], None]] = []
        self.content: str = ""
        self.lines: List[str] = []
        self.reloadAndReset()

    def _relativeImportChecks(self):
        for i, line in enumerate(self.lines):
            imports = re.findall(_RELATIVE_IMPORT_REGEX, line)
            for imp in imports:
                import_path = self.dir / imp
                if not import_path.exists():
                    self.errors.append(
                        f"Relative import `{imp}` does not exist: {self.path}:{i + 1}"
                    )
                    self.fixes.append(
                        lambda i=i, imp=imp: self._fixRelativeImport(i, imp)
                    )

    def _fixRelativeImport(self, line_index: int, import_path: str):
        self.lines[line_index] = self.lines[line_index].replace(
            f'"{import_path}"', f"<{import_path}>"
        )

    def applyFixes(self) -> None:
        if self.fixes:
            for fix in self.fixes:
                fix()
            with open(self.path, "w") as f:
                f.writelines(self.lines)

    def getFixCount(self):
        return len(self.fixes)

    def _loadFromDisk(self) -> None:
        """Load file content from disk."""
        with open(self.path, "r") as file:
            self.content = file.read()
        self.lines = self.content.splitlines(keepends=True)

    def reloadAndReset(self):
        """Reload file content from disk and reset error/fix lists. Returns True on success."""
        self._loadFromDisk()
        self.errors = []
        self.fixes = []

    def verify(self, verbose: bool) -> bool:
        """Returns True if all checks passed, False otherwise"""

        self._relativeImportChecks()

        if len(self.errors) > 0:
            log_new_line()
            log_warning(f"{self.path}: ERRORS FOUND")
            for error in self.errors:
                log_warning(" * " + error)
            return False
        else:
            if verbose:
                log_good(f"{self.path}: OK")
            return True


def get_source_files(
    all: bool, relative_to: str, no_merge_base: bool
) -> List[SourceFile]:
    files = get_files_for_linter(all, relative_to, no_merge_base).keys()
    source_files: List[SourceFile] = []
    for file in files:
        if file.endswith(".cpp") or file.endswith(".hpp"):
            source_files.append(SourceFile(file))
    return source_files
