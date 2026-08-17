import re
import json
from .helpers import (
    BashCommandError,
    log_good,
    log_info,
    log_warning,
    log_new_line,
    bash_command_get_output,
)
import os

from .list_files import list_files_impl


def issue_checker_impl(
    issues: list[str] | None, branch: str = "origin/main", no_merge_base: bool = False
) -> bool:
    """
    Checks if specified GitHub issue numbers appear in the codebase.

    Args:
        issues: List of issue numbers to search for in the code
        branch: Git branch to check against (default: origin/main)
        no_merge_base: If True, skip merge base calculation

    Returns:
        bool: True if no issue references found, False if issues were found in code
    """
    if not issues:
        issues = get_issues_from_github()
        if not issues:
            log_info("No issue numbers to look for in the code. Nothing to check.")
            return True

    valid_issue_numbers: list[str] = []
    for num in issues:
        num_str: str = str(num).strip()
        if not num_str:
            continue
        if not num_str.isdigit() or int(num_str) <= 0:
            log_warning(
                f"Issue number '{num_str}' is not a positive integer. Skipping."
            )
            continue
        valid_issue_numbers.append(num_str)
    if not valid_issue_numbers:
        log_warning("No valid issue numbers provided.")
        return True

    # Match #number followed by a non-digit (whitespace, punctuation, or end of line)
    patterns = [
        re.compile(rf"#\b({re.escape(num)})(?!\d)") for num in valid_issue_numbers
    ]

    files = list_files_impl(branch=branch, no_merge_base=no_merge_base)

    found_any = False
    summary = {num: 0 for num in valid_issue_numbers}

    for file in files:
        if os.path.isdir(file):
            continue
        try:
            with open(file, "r") as f:
                for i, line in enumerate(f, 1):
                    for idx, pat in enumerate(patterns):
                        if pat.search(line):
                            log_warning(f"{file}:{i}: {line.strip()}")
                            summary[valid_issue_numbers[idx]] += 1
                            found_any = True
        except Exception:
            continue

    log_new_line()
    log_info("Summary:")
    for num in valid_issue_numbers:
        log_info(f"#{num}: {summary[num]} occurrence(s)")

    if found_any:
        return False

    log_good("No references to the checked issue(s) were found in the code")
    return True


def get_issues_from_github() -> list[str]:
    """
    Retrieves open issue numbers from GitHub for the current repository and PR.

    This function:
    1. Checks if 'gh' CLI is available
    2. Extracts repository owner/name from git remote
    3. Gets current branch name
    4. Finds associated Pull Request number (from PR_NUMBER env var or gh CLI)
    5. Queries GitHub GraphQL API for issues that would be closed by the PR

    Returns:
        list[str]: List of open issue numbers as strings, empty list if none found or on error
    """
    import shutil

    # Check if 'gh' is available
    if shutil.which("gh") is None:
        log_warning(
            "'gh' CLI not found. Cannot run issue-checker without 'gh' or issue numbers."
        )
        return []

    # Get OWNER and REPO from git remote (support both SSH and HTTPS URLs)
    try:
        remote_url, _ = bash_command_get_output("git remote get-url origin")
        remote_url = remote_url.strip()
    except BashCommandError as e:
        log_warning(f"Could not get git remote url: {e}")
        return []

    m = re.match(
        r"(?:git@|https://)([^/:]+)[:/]+([^/]+)/([^/.]+)(?:\.git)?", remote_url
    )
    if not m:
        log_warning("Could not parse OWNER/REPO from git remote.")
        return []
    owner, repo = m.group(2), m.group(3)

    # Get current branch
    try:
        branch_name, _ = bash_command_get_output("git rev-parse --abbrev-ref HEAD")
        branch_name = branch_name.strip()
    except BashCommandError as e:
        log_warning(f"Could not get current branch: {e}")
        return []

    # Check for PR_NUMBER in environment (used in CI workflows)
    pr_number = os.environ.get("PR_NUMBER")
    if not pr_number:
        # If not set, try to get PR number associated with this branch using gh
        try:
            pr_number, _ = bash_command_get_output(
                f"gh pr view {branch_name} --json number -q .number"
            )
            pr_number = pr_number.strip()
            log_info(f"Found associated Pull Request number: {pr_number}")
        except BashCommandError as e:
            log_warning(
                f"No associated Pull Request found for branch: {branch_name}: {e}"
            )
            return []

    if not pr_number:
        log_warning(f"No associated Pull Request found for branch: {branch_name}")
        return []

    # Prepare GraphQL query string with variables substituted
    query = f"""{{
      repository(owner: "{owner}", name: "{repo}") {{
        pullRequest(number: {pr_number}) {{
          closingIssuesReferences(first: 100) {{
            nodes {{ number }}
          }}
        }}
      }}
    }}"""

    gh_cmd = f"gh api graphql -f 'query={query}'"
    try:
        gh_output, _ = bash_command_get_output(gh_cmd)
        data = json.loads(gh_output)
        nodes = (
            data.get("data", {})
            .get("repository", {})
            .get("pullRequest", {})
            .get("closingIssuesReferences", {})
            .get("nodes", [])
        )
        return [
            str(node["number"])
            for node in nodes
            if node is not None and "number" in node
        ]
    except BashCommandError as e:
        log_warning(f"Error while fetching issue numbers via gh api: {e}")
        return []
