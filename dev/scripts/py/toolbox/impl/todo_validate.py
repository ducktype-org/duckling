import re
import os
import json
import shutil
from .helpers import (
    BashCommandError,
    log_info,
    log_warning,
    bash_command_get_output,
    exit_with_error,
)
from .list_files import list_files_impl


def check_issue_exists_and_open(issue_number: str) -> bool:
    """
    Check if a specific GitHub issue exists and is open.

    Args:
        issue_number: The issue number to check

    Returns:
        bool: True if the issue exists and is open, False otherwise
    """
    # Check if 'gh' is available
    if shutil.which("gh") is None:
        log_warning("'gh' CLI not found. Cannot validate issue existence.")
        return False

    # Get OWNER and REPO from git remote (support both SSH and HTTPS URLs)
    remote_url, _ = bash_command_get_output("git remote get-url origin")
    remote_url = remote_url.strip()

    m = re.match(
        r"(?:git@|https://)([^/:]+)[:/]+([^/]+)/([^/.]+)(?:\.git)?", remote_url
    )
    if not m:
        exit_with_error("Could not parse OWNER/REPO from git remote.")
    owner, repo = m.group(2), m.group(3)

    # Query for specific issue using GraphQL
    query = """
    query($owner: String!, $repo: String!, $issue_number: Int!) {
      repository(owner: $owner, name: $repo) {
        issue(number: $issue_number) {
          number
          state
        }
      }
    }
    """

    gh_cmd = (
        "gh api graphql "
        f"-f query='{query}' "
        f"-f owner='{owner}' "
        f"-f repo='{repo}' "
        f"-F issue_number={issue_number}"
    )
    try:
        gh_output, _ = bash_command_get_output(gh_cmd)
        data = json.loads(gh_output)
        issue = data.get("data", {}).get("repository", {}).get("issue")

        if issue is None:
            return False  # Issue doesn't exist

        return issue.get("state") == "OPEN"

    except BashCommandError as e:
        exit_with_error(f"During Github's API query: {e.stderr}")

    except json.JSONDecodeError as e:
        log_warning(f"Error parsing GitHub API response: {e}")
        return False


def todo_validate_impl(
    branch: str = "origin/main",
    no_merge_base: bool = False,
    exclude_files: list[str] = None,  # None here is on purpose
    all: bool = False,
) -> bool:
    """
    Validates that all TODO/FIXME comments follow the required format with issue numbers.

    Args:
        branch: Git branch to check against (default: origin/main)
        no_merge_base: If True, skip merge base calculation
        exclude_files: List of file path suffixes, that should be excluded
        all: If True, scan entire project, otherwise just the difference

    Returns:
        bool: True if all TODOs are properly formatted, False if any violations are found.
    """

    # Define strict patterns for TODO/FIXME comments in the required format
    # Format: @TODO: #123 description or @FIXME #123 description (only @ prefixed)
    if exclude_files is None:
        exclude_files = []
    todo_patterns = [
        re.compile(r"@TODO:\s+#(\d+)\s+\S+", re.IGNORECASE),
        re.compile(r"@FIXME:\s+#(\d+)\s+\S+", re.IGNORECASE),
    ]

    # Chcemy wykrywać wszystkie TODO

    # Patterns to detect any TODO/FIXME comment (for reporting violations)
    any_todo_patterns = [
        re.compile(r"[^!]@TODO", re.IGNORECASE),
        re.compile(r"[^!]@FIXME", re.IGNORECASE),
        re.compile(r"(?!!@)[^!]TODO[^_-]", re.IGNORECASE),
        re.compile(r"(?!!@)[^!]FIXME[^_-]", re.IGNORECASE),
        re.compile(r"(?!!@)[^!]TODO\s", re.IGNORECASE),
        re.compile(r"(?!!@)[^!]FIXME\s", re.IGNORECASE),
    ]

    files_and_lines: dict[str, list[tuple[int, int]]] = list_files_impl(
        only_modified=not all, lines=True, branch=branch, no_merge_base=no_merge_base
    ) # type: ignore
    print(files_and_lines)

    # Pop the current file, so that the verification can pass
    for file in list(files_and_lines.keys()):
        try:
            if __file__.endswith(file):
                files_and_lines.pop(file)
            if any(file.endswith(suffix) for suffix in exclude_files):
                files_and_lines.pop(file)
        except KeyError:
            pass

    # Check only modified files and lines
    violations_found = False
    violation_count = 0
    invalid_issue_count = 0

    log_info("Validating TODO/FIXME comments format...")

    for file, line_ranges in files_and_lines.items():
        if os.path.isdir(file):
            continue

        try:
            with open(file, "r", encoding="utf-8", errors="ignore") as f:
                file_lines = f.readlines()

                for start_line, end_line in line_ranges:
                    for line_num in range(start_line, end_line):
                        if line_num <= len(file_lines):
                            line = file_lines[
                                line_num - 1
                            ]  # Convert to 0-based indexing

                            # Check if line contains any TODO/FIXME pattern
                            has_todo = any(
                                pattern.search(line) for pattern in any_todo_patterns
                            )

                            if has_todo:
                                # Check if it follows the strict format
                                valid_format = False
                                issue_number = None

                                for pattern in todo_patterns:
                                    match = pattern.search(line)
                                    if match:
                                        valid_format = True
                                        issue_number = match.group(1)
                                        break

                                if not valid_format:
                                    log_warning(f"{file}:{line_num}: {line.strip()}")
                                    violations_found = True
                                    violation_count += 1
                                elif issue_number and not check_issue_exists_and_open(
                                    issue_number
                                ):
                                    log_warning(
                                        f"{file}:{line_num}: Issue #{issue_number} does not exist or is not open: {line.strip()}"
                                    )
                                    violations_found = True
                                    invalid_issue_count += 1
                        else:
                            exit_with_error(f"Line count in {file} has changed!")

        except OSError:
            # Skip files that can't be read (binary files, permission issues, etc.)
            exit_with_error(
                f"Cannot read {file}. Make sure it's text-readable and you have correct permissions."
            )

    if violations_found:
        if violation_count > 0:
            log_warning(
                f"Found {violation_count} TODO/FIXME comment(s) without proper format"
            )
        if invalid_issue_count > 0:
            log_warning(
                f"Found {invalid_issue_count} TODO/FIXME comment(s) with invalid or closed issue numbers"
            )
        log_info(
            "All TODO/FIXME comments must follow the format: @TODO: #issue_number description"
        )
        log_info("Example: // @TODO: #0123 Implement this feature")
        log_info("The issue number must reference an open GitHub issue.")
    else:
        log_info(
            "All TODO/FIXME comments are properly formatted with valid issue numbers"
        )

    return not violations_found
