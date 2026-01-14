from typing import List
import re
from pathlib import Path

from .cpp_linter import get_files_for_linter
from .helpers import (
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
):
    passed_all = True
    files_with_fixes = []
    files_with_errors = []

    files = get_source_files(all, branch, no_merge_base)
    for f in files:
        passed = f.runAllChecks(verbose)
        if not passed:
            passed_all = False
            files_with_errors.append(f)
        if len(f.fixes) > 0:
            files_with_fixes.append(f)

    if len(files_with_fixes) > 0 and not no_fix:
        total_fixes = sum(len(f.fixes) for f in files_with_fixes)
        log_new_line()
        log_info(
            f"Can perform {total_fixes} automatic fixes across {len(files_with_fixes)} files."
        )
        try:
            response = get_input("Do you want to apply these fixes? [Y/n]")
            # Default to yes if empty response
            if (response or '').lower() not in ['n', 'no']:
                for f in files_with_fixes:
                    f.applyFixes()
                log_info("Fixes applied!")
                # Re-check files that had fixes applied
                passed_all = True
                for f in files_with_errors:
                    if f in files_with_fixes:
                        # This file was fixed, reload and re-check
                        f.reloadAndReset()
                        if not f.runAllChecks(verbose):
                            passed_all = False
                    else:
                        # This file had errors but no automatic fix
                        passed_all = False
        except EOFError:
            pass

    return passed_all


class SourceFile:
    def __init__(self, path: str):
        self.path = Path(path)
        self.dir = self.path.parent
        self.errors = []
        self.fixes = []
        with open(path, "r") as file:
            self.content = file.read()
        self.lines = self.content.splitlines(keepends=True)

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

    def _fixRelativeImport(self, line_index, import_path):
        self.lines[line_index] = self.lines[line_index].replace(
            f'"{import_path}"', f"<{import_path}>"
        )

    def applyFixes(self):
        for fix in self.fixes:
            fix()
        with open(self.path, "w") as f:
            f.writelines(self.lines)
    
    def reload(self):
        """Reload file content from disk"""
        with open(self.path, "r") as file:
            self.content = file.read()
        self.lines = self.content.splitlines(keepends=True)
    
    def reloadAndReset(self):
        """Reload file content from disk and reset error/fix lists"""
        self.reload()
        self.errors = []
        self.fixes = []

    def runAllChecks(self, verbose):
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
                log_info(f"{self.path}: OK")
            return True


def get_source_files(all, relative_to, no_merge_base) -> List[SourceFile]:
    files = get_files_for_linter(all, relative_to, no_merge_base).keys()
    source_files = []
    for file in files:
        if file.endswith(".cpp") or file.endswith(".hpp"):
            source_files.append(SourceFile(file))
    return source_files
