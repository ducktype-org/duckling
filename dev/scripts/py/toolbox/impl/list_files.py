import os
import shlex

from .helpers import (
    bash_command_get_output,
    log_warning,
)


def list_files_impl(
    extensions: list[str] | None = None,
    branch: str = "origin/main",
    only_modified: bool = False,
    no_merge_base: bool = False,
    lines: bool = False,
) -> list[str] | dict[str, list[tuple[int, int]]]:
    """
    List files in the repository based on the specified criteria.

    Args:
        extensions: List of file extensions to include (e.g., ['.cpp', '.hpp', '.py'])
                   If None, all files are included.
        branch: The branch to compare against for modified files (ignored if only_modified=False)
        only_modified: If True, list only modified files; if False, list all tracked files
        no_merge_base: If True, compare against latest commit on branch instead of merge base
        lines: If True and only_modified=True, return dict with file->line_ranges mapping

    Returns:
        List of file paths relative to the current working directory when lines=False,
        or Dict mapping file paths to list of (start_line, end_line) tuples when lines=True
    """
    if not only_modified:
        if lines:
            # For all files with lines, we return all lines in each file
            files = _get_all_tracked_files(extensions)
            result: dict[str, list[tuple[int, int]]] = {}
            for file in files:
                # Skip directories (e.g., git submodules)
                if os.path.isdir(file):
                    continue
                line_count_str, _ = bash_command_get_output(
                    f"wc -l < {shlex.quote(file)}"
                )
                line_count = int(line_count_str.strip())
                result[file] = [(1, line_count)]
            return result
        else:
            return _get_all_tracked_files(extensions)
    else:
        if lines:
            return _get_modified_files_and_lines(extensions, branch, no_merge_base)
        else:
            return _get_modified_files(extensions, branch, no_merge_base)


def _get_all_tracked_files(extensions: list[str] | None = None) -> list[str]:
    """Get all tracked files in the repository."""
    files_str, _ = bash_command_get_output("git ls-tree -r --name-only HEAD")
    files = [f.strip() for f in files_str.strip().split("\n") if f.strip()]

    return _filter_by_extensions(files, extensions)


def _get_modified_files(
    extensions: list[str] | None = None,
    branch: str = "origin/main",
    no_merge_base: bool = False,
) -> list[str]:
    """Get modified files compared to the specified branch."""
    # Check for unstaged new files and warn about them
    unstaged_files = _get_unstaged_new_files()
    if unstaged_files:
        log_warning(
            f"Files not in working tree, so not included in diff: [{', '.join(unstaged_files)}]"
        )

    # Get the diff
    diff_out, _ = bash_command_get_output(
        f"git diff {'' if no_merge_base else '--merge-base'} {branch} --name-only --relative"
    )
    files = [f.strip() for f in diff_out.strip().split("\n") if f.strip()]

    return _filter_by_extensions(files, extensions)


def _get_unstaged_new_files() -> list[str]:
    """Get list of files that are new but not staged."""
    status_out, _ = bash_command_get_output("git status --porcelain")
    new_unstaged_files = []
    for file in status_out.splitlines():
        if file.startswith("??"):
            new_unstaged_files.append(file[3:])
    return new_unstaged_files


def _get_modified_files_and_lines(
    extensions: list[str] | None = None,
    branch: str = "origin/main",
    no_merge_base: bool = False,
) -> dict[str, list[tuple[int, int]]]:
    """Get modified files and their line ranges compared to the specified branch."""
    # Check for unstaged new files and warn about them
    unstaged_files = _get_unstaged_new_files()
    if unstaged_files:
        log_warning(
            f"Files not in working tree, so not included in diff: [{', '.join(unstaged_files)}]"
        )

    # Get the diff with line ranges
    diff_out, _ = bash_command_get_output(
        f"git diff {'' if no_merge_base else '--merge-base'} {branch} -U0 --relative"
    )
    diff_lines = diff_out.splitlines()

    changes = {}
    prev_line = None
    filename = None

    for line in diff_lines:
        file_deleted = prev_line == "+++ /dev/null"

        if line.startswith("@@") and not file_deleted:
            # Check if this diff is for a new file...
            if prev_line and prev_line.startswith("+++ b/"):
                # File has changed
                filename = prev_line[len("+++ b/") :].rstrip()

            if filename is None:
                continue

            # Parse diffed lines:
            # @@ -{line_start},{num_lines} +{line_start},{num_lines} @@ ...
            # or
            # @@ -{line_start} +{line_start} @@ ...
            diffed = line[line.find("+") + 1 :]
            diffed = diffed[: diffed.find("@@")]

            line_range = None
            if "," in diffed:
                # In this case there were multiple lines changed in format: +{line_start},{num_lines}
                line_start, num_lines = diffed.split(",")
                line_start = int(line_start)
                num_lines = int(num_lines)
                line_range = (line_start, line_start + num_lines)

                # This is for the format (note the "0" added lines):
                # @@ -16 +15,0 @@
                # -#include <iostream>
                # (Which is very odd)
                if line_range[1] == line_range[0]:
                    continue
            else:
                # In this case there is only 1 line changed
                line_start = int(diffed)
                line_range = (line_start, line_start + 1)

            # Save to our dict which files have changed and which haven't.
            # There may be multiple places in one files with changed lines, so we have a list of ranges.
            if filename not in changes:
                changes[filename] = []

            changes[filename].append(line_range)

        prev_line = line

    # Filter by extensions if specified
    if extensions:
        filtered_changes = {}
        for file, line_ranges in changes.items():
            if _file_matches_extensions(file, extensions):
                filtered_changes[file] = line_ranges
        return filtered_changes

    return changes


def _file_matches_extensions(file: str, extensions: list[str]) -> bool:
    """Check if a file matches any of the specified extensions."""
    # Normalize extensions to ensure they start with a dot
    normalized_extensions = []
    for ext in extensions:
        if not ext.startswith("."):
            ext = "." + ext
        normalized_extensions.append(ext.lower())

    # Check if file has one of the desired extensions
    file_ext = os.path.splitext(file)[1].lower()
    return file_ext in normalized_extensions


def _filter_by_extensions(files: list[str], extensions: list[str] | None) -> list[str]:
    """Filter files by the specified extensions."""
    if not extensions:
        return files

    filtered_files = []
    for file in files:
        # Skip directories
        if os.path.isdir(file):
            continue

        # Check if file has one of the desired extensions
        if _file_matches_extensions(file, extensions):
            filtered_files.append(file)

    return filtered_files
