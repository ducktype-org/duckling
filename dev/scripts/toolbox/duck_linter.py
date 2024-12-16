from typing import List
import re
from pathlib import Path

from scripts.toolbox.cpp_linter import get_files_for_linter
from scripts.toolbox.helpers import log_info, log_warning, log_new_line

_RELATIVE_IMPORT_REGEX = re.compile(r'#include "(.*?)"')


class SourceFile:
    def __init__(self, path: str):
        self.path = Path(path)
        self.dir = self.path.parent
        self.errors = []
        with open(path, "r") as file:
            self.content = file.read()

    def _relativeImportChecks(self):
        imports = re.findall(_RELATIVE_IMPORT_REGEX, self.content)
        for imp in imports:
            import_path = self.dir / imp
            if not import_path.exists():
                self.errors.append(f"Relative import `{imp}` does not exist.")

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


def get_source_files(all, relative_to) -> List[SourceFile]:
    files = get_files_for_linter(all, relative_to).keys()
    source_files = []
    for file in files:
        if file.endswith(".cpp") or file.endswith(".hpp"):
            source_files.append(SourceFile(file))
    return source_files


def duck_linter_impl(all, branch, verbose):
    passed_all = True

    file = get_source_files(all, branch)
    for f in file:
        passed = f.runAllChecks(verbose)
        if not passed:
            passed_all = False

    return passed_all
