"""
GitHub creates no runs for a workflow whose file does not parse as YAML —
no failure, no annotation on the PR, other workflows keep running. The
breakage is invisible until someone notices runs are missing, so validity
has to be checked outside GitHub: locally (pr-validate) and by a job in a
different workflow file (linting.yml).
"""

from pathlib import Path

import yaml

from .helpers import (
    click_log,
    log_info,
)

# impl/ -> toolbox/ -> py/ -> scripts/ -> dev/ -> repo root
_WORKFLOWS_DIR = Path(__file__).resolve().parents[5] / ".github" / "workflows"


def workflows_lint_impl() -> bool:
    """
    Validates that every GitHub workflow file parses as YAML.

    A workflow file that does not parse is not reported by GitHub on
    pull_request events - the workflow silently creates no runs at all
    until the file is fixed.

    Returns True if all workflow files are valid, False otherwise.
    """

    workflow_files = sorted(
        path
        for path in _WORKFLOWS_DIR.iterdir()
        if path.suffix in (".yml", ".yaml") and path.is_file()
    )

    ok = True
    for path in workflow_files:
        try:
            yaml.safe_load(path.read_text())
        except yaml.YAMLError as error:
            click_log("ERROR", f"{path} is not valid YAML:\n{error}", fg="red", bold=True)
            ok = False

    if ok:
        log_info(f"All {len(workflow_files)} workflow file(s) parsed successfully")
    return ok
