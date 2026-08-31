import os
import sys

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
    include_untracked: bool = False,
) -> list[str] | dict[str, list[tuple[int, int]]]:
    """
    List files in the repository based on the specified criteria.

    Args:
        extensions: List of file extensions to include (e.g., ['.cpp', '.hpp', '.py'])
                   If None, all files are included.
        branch: The branch to compare against for modified files (ignored if only_modified=False)
        only_modified: If True, list only modified files; if False, list all tracked files
        no_merge_base: If True, compare against latest commit on branch instead of merge base
        lines: If True, return dict with file->line_ranges mapping
        include_untracked: If True, also list untracked files (counted as fully modified
                   when lines=True); if False, and with only_modified=True, only warn
                   that they were left out

    Returns:
        List of file paths relative to the current working directory when lines=False,
        or Dict mapping file paths to list of (start_line, end_line) tuples when lines=True
    """
    if not only_modified:
        files = _get_all_tracked_files(extensions)
        if include_untracked:
            files += _get_untracked_files(extensions)
        if lines:
            # For all files with lines, we return all lines in each file
            return _get_full_line_ranges(files)
        else:
            return files
    else:
        if lines:
            return _get_modified_files_and_lines(
                extensions, branch, no_merge_base, include_untracked
            )
        else:
            return _get_modified_files(
                extensions, branch, no_merge_base, include_untracked
            )


def _get_all_tracked_files(extensions: list[str] | None = None) -> list[str]:
    """
    Get all tracked files below the current directory, relative to it.

    The index, not HEAD: a file that is staged but not committed yet is tracked, and a
    "format/lint everything" caller wants it.
    """
    files_str, _ = bash_command_get_output("git ls-files --cached")
    files = [f.strip() for f in files_str.strip().split("\n") if f.strip()]

    return _filter_by_extensions(files, extensions)


def _get_modified_files(
    extensions: list[str] | None = None,
    branch: str = "origin/main",
    no_merge_base: bool = False,
    include_untracked: bool = False,
) -> list[str]:
    """Get modified files compared to the specified branch."""
    # Get the diff. `--diff-filter=d` drops the files the branch DELETED: every caller wants to
    # read or rewrite the files it gets back, and a deleted path is not there anymore, so listing
    # it just makes the caller fail (clang-format reports "No such file or directory" and
    # `format_repo_cpp.sh` returns non-zero on a branch that removed a source file).
    diff_out, _ = bash_command_get_output(
        f"git diff {'' if no_merge_base else '--merge-base'} {branch} "
        "--name-only --relative --diff-filter=d"
    )
    files = [f.strip() for f in diff_out.strip().split("\n") if f.strip()]

    return _filter_by_extensions(files, extensions) + _get_untracked_files_to_list(
        extensions, include_untracked
    )


def _get_untracked_files(extensions: list[str] | None = None) -> list[str]:
    """
    Get the untracked files below the current directory, gitignored ones excluded.

    The paths are relative to the current working directory, just like the ones
    `git diff --relative` reports, so both listings can be concatenated.
    """
    files_str, _ = bash_command_get_output("git ls-files --others --exclude-standard")
    files = [f.strip() for f in files_str.strip().split("\n") if f.strip()]

    return _filter_by_extensions(files, extensions)


def _get_untracked_files_to_list(
    extensions: list[str] | None, include_untracked: bool
) -> list[str]:
    """
    Get the untracked files to append to a modified files listing.

    `git diff` never reports them, so they are opt-in; when left out they are warned
    about instead, on stderr — `list-files`' stdout is a machine readable file list,
    and a warning mixed into it gets consumed as a file name.
    """
    untracked_files = _get_untracked_files(extensions)
    if not untracked_files:
        return []

    if not include_untracked:
        log_warning(
            "Untracked files are not part of the diff, so they were skipped: "
            f"[{', '.join(untracked_files)}]",
            file=sys.stderr,
        )
        return []

    return untracked_files


def _get_full_line_ranges(files: list[str]) -> dict[str, list[tuple[int, int]]]:
    """
    Map each of the files to a single range covering all of its lines.

    Counted in Python, not with `wc -l`: that counts *newlines*, so a file without a
    trailing newline would be one line short and a one-line file would give the
    inverted range `1-0`, which consumers pass on verbatim (`--lines=1:0` is an error).
    """
    result: dict[str, list[tuple[int, int]]] = {}
    for file in files:
        # Skip directories (e.g. git submodules) and index entries whose working-tree
        # copy is gone (deleted, not staged yet)
        if not os.path.isfile(file):
            continue
        with open(file, "rb") as f:
            line_count = len(f.read().splitlines())
        if not line_count:
            continue
        result[file] = [(1, line_count)]
    return result


def _get_modified_files_and_lines(
    extensions: list[str] | None = None,
    branch: str = "origin/main",
    no_merge_base: bool = False,
    include_untracked: bool = False,
) -> dict[str, list[tuple[int, int]]]:
    """Get modified files and their line ranges compared to the specified branch."""
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
        changes = filtered_changes

    # An untracked file has no diff to parse, so all of its lines count as changed
    changes.update(
        _get_full_line_ranges(
            _get_untracked_files_to_list(extensions, include_untracked)
        )
    )

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
