import re
import os
import json
import shutil
from dataclasses import dataclass
from collections.abc import Iterator
from typing import Callable
from .helpers import (
    BashCommandError,
    log_info,
    log_warning,
    bash_command_get_output,
    exit_with_error,
)
from .list_files import list_files_impl


# Define strict patterns for TODO/FIXME comments in the required format
# Format: @TODO: #123 description or @FIXME #123 description (only @ prefixed)
VALID_TODO_PATTERNS = [
    re.compile(r"@TODO:\s+#(\d+)\s+\S+", re.IGNORECASE),
    re.compile(r"@FIXME:\s+#(\d+)\s+\S+", re.IGNORECASE),
]

# Patterns to detect any TODO/FIXME comment (for reporting violations)
ANY_TODO_PATTERNS = [
    re.compile(r"[^!]@TODO", re.IGNORECASE),
    re.compile(r"[^!]@FIXME", re.IGNORECASE),
    re.compile(r"(?!!@)[^!]TODO[^_-]", re.IGNORECASE),
    re.compile(r"(?!!@)[^!]FIXME[^_-]", re.IGNORECASE),
    re.compile(r"(?!!@)[^!]TODO\s", re.IGNORECASE),
    re.compile(r"(?!!@)[^!]FIXME\s", re.IGNORECASE),
]


@dataclass(frozen=True)
class TodoLine:
    path: str
    line_num: int
    content: str


def iter_todo_lines(
    files_and_lines: dict[str, list[tuple[int, int]]],
    *,
    handled_exceptions: tuple[type[BaseException], ...] = (OSError,),
    error_handler: Callable[[str, BaseException], None] | None = None,
) -> Iterator[TodoLine]:
    for file, line_ranges in files_and_lines.items():
        if os.path.isdir(file):
            continue

        try:
            with open(file, "r", encoding="utf-8", errors="ignore") as f:
                file_lines = f.readlines()
        except handled_exceptions as exc:  # type: ignore[misc]
            if error_handler is not None:
                error_handler(file, exc)
            continue

        for start_line, end_line in line_ranges:
            for line_num in range(start_line, end_line):
                if line_num <= len(file_lines):
                    yield TodoLine(file, line_num, file_lines[line_num - 1])
                else:
                    exit_with_error(f"Line count in {file} has changed!")


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
    exclude_files: list[str] | None = None,  # None here is on purpose
    all: bool = False,
    print_todos: bool = False,
) -> bool:
    """
    Validates that all TODO/FIXME comments follow the required format with issue numbers.

    Args:
        branch: Git branch to check against (default: origin/main)
        no_merge_base: If True, skip merge base calculation
        exclude_files: List of file path suffixes, that should be excluded
        all: If True, scan entire project, otherwise just the difference
        print_todos: If True, print newly added TODOs with issue numbers

    Returns:
        bool: True if all TODOs are properly formatted, False if any violations are found.
    """

    if exclude_files is None:
        exclude_files = ["todo_validate.py", "todo_counter.py", "CLAUDE.md"]

    files_and_lines: dict[str, list[tuple[int, int]]] = list_files_impl(
        only_modified=not all, lines=True, branch=branch, no_merge_base=no_merge_base
    ) # type: ignore

    # Pop the current file, so that the verification can pass
    for file in list(files_and_lines.keys()):
        try:
            if __file__.endswith(file):
                files_and_lines.pop(file)
            if any(file.endswith(suffix) for suffix in exclude_files):
                files_and_lines.pop(file)
        except KeyError:
            pass

    # Handle --print-todos flag
    if print_todos:
        issues = get_todos_from_lines(files_and_lines)
        if issues:
            print(" ".join(f"#{issue}" for issue in issues))
        else:
            print("No new TODOs with issue numbers found.")
        return True

    # Check only modified files and lines
    violations_found = False
    violation_count = 0
    invalid_issue_count = 0

    log_info("Validating TODO/FIXME comments format...")

    def _strict_error_handler(file_path: str, _exc: BaseException) -> None:
        exit_with_error(
            f"Cannot read {file_path}. Make sure it's text-readable and you have correct permissions."
        )

    for todo_line in iter_todo_lines(
        files_and_lines,
        error_handler=_strict_error_handler,
    ):
        line = todo_line.content

        # Check if line contains any TODO/FIXME pattern
        has_todo = any(pattern.search(line) for pattern in ANY_TODO_PATTERNS)

        if has_todo:
            # Check if it follows the strict format
            valid_format = False
            issue_number = None

            for pattern in VALID_TODO_PATTERNS:
                match = pattern.search(line)
                if match:
                    valid_format = True
                    issue_number = match.group(1)
                    break

            if not valid_format:
                log_warning(
                    f"{todo_line.path}:{todo_line.line_num}: {line.strip()}"
                )
                violations_found = True
                violation_count += 1
            elif issue_number and not check_issue_exists_and_open(issue_number):
                log_warning(
                    f"{todo_line.path}:{todo_line.line_num}: Issue #{issue_number} does not exist or is not open: {line.strip()}"
                )
                violations_found = True
                invalid_issue_count += 1

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


def get_todos_from_lines(files_and_lines: dict[str, list[tuple[int, int]]]) -> list[str]:
    """
    Returns newly added TODOs with issue numbers in modified files.

    Scans modified files for TODO/FIXME comments that contain issue numbers.

    Args:
        files_and_lines: Dictionary mapping file paths to line ranges to scan
    """
    found_issues = set()

    def _skip_error_handler(_path: str, _exc: BaseException) -> None:
        return

    for todo_line in iter_todo_lines(
        files_and_lines,
        handled_exceptions=(IOError, OSError, UnicodeDecodeError),
        error_handler=_skip_error_handler,
    ):
        for pattern in VALID_TODO_PATTERNS:
            match = pattern.search(todo_line.content)
            if match:
                found_issues.add(match.group(1))
    sorted_issues = sorted(found_issues, key=int)
    return sorted_issues
