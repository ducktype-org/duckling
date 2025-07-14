from typing import List
import re
from pathlib import Path
from click import option, command

from .cpp_linter import get_files_for_linter
from .helpers import (
    log_info, 
    log_warning, 
    log_new_line, 
    exit_with_error
)

_RELATIVE_IMPORT_REGEX = re.compile(r'#include "(.*?)"')


def duck_linter_impl(
    all: bool = False,
    branch: str = "origin/main",
    verbose: bool = False,
    no_merge_base: bool = False,
):
    passed_all = True

    file = get_source_files(all, branch, no_merge_base)
    for f in file:
        passed = f.runAllChecks(verbose)
        if not passed:
            passed_all = False

    return passed_all


class SourceFile:
    def __init__(self, path: str):
        self.path = Path(path)
        self.dir = self.path.parent
        self.errors = []
        with open(path, "r") as file:
            self.content = file.read()

    def _relativeImportChecks(self):
        lines = self.content.splitlines()
        for i, line in enumerate(lines):
            imports = re.findall(_RELATIVE_IMPORT_REGEX, line)
            for imp in imports:
                import_path = self.dir / imp
                if not import_path.exists():
                    self.errors.append(
                        f"Relative import `{imp}` does not exist: {self.path}:{i + 1}"
                    )

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

@command()
@option(
    "-a",
    "--all",
    is_flag=True,
    default=False,
    help="Check all files, not just the ones that are modified",
)
@option(
    "-r",
    "--branch",
    help="The branch relative to which the diff is created.",
    type=str,
    default="origin/main",
)
@option(
    "-v",
    "--verbose",
    is_flag=True,
    default=False,
    help="Also shows checks files that didn't had any errors.",
)
@option(
    "--no-merge-base",
    is_flag=True,
    default=False,
    help="On no-merge-base: compare against the latest commit on `branch` "
    "instead of the commit which is the LCA of `branch` and current branch. "
    "This feature allows to run the linter on a shallow clone.",
)
def duck_linter(*args, **kwargs):
    """Check for violations of
    some of the C++ coding guidelines for Duckling project.
    Current checks:
    * relative import check

    For details see dev-guides.
    """
    passed = duck_linter_impl(*args, **kwargs)
    if not passed:
        exit_with_error("Linting failed.")

if __name__ == "__main__":
    if Path.cwd().name != "dev":
        exit_with_error("Please run this script from the dev/ directory.")

    duck_linter()