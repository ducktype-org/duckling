from ..impl.helpers import exit_with_error
from ..impl.workflows_lint import workflows_lint_impl
from click import command


@command()
def workflows_lint():
    """Validate that all GitHub workflow files parse as YAML.

    GitHub silently stops creating runs for a workflow whose file does
    not parse, so a broken workflow never reports its own breakage.
    """
    if not workflows_lint_impl():
        exit_with_error("Workflow YAML validation failed.")
