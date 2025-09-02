import os
from typing import List, Optional

from .helpers import (
    bash_command_get_output,
    log_warning,
)


def list_files_impl(
    extensions: Optional[List[str]] = None,
    branch: str = "origin/main",
    all: bool = False,
    no_merge_base: bool = False,
) -> List[str]:
    """
    List files in the repository based on the specified criteria.
    
    Args:
        extensions: List of file extensions to include (e.g., ['.cpp', '.hpp', '.py'])
                   If None, all files are included.
        branch: The branch to compare against for modified files (ignored if all=True)
        all: If True, list all tracked files; if False, list only modified files
        no_merge_base: If True, compare against latest commit on branch instead of merge base
        
    Returns:
        List of file paths relative to the repository root
    """
    if all:
        return _get_all_tracked_files(extensions)
    else:
        return _get_modified_files(extensions, branch, no_merge_base)


def _get_all_tracked_files(extensions: Optional[List[str]] = None) -> List[str]:
    """Get all tracked files in the repository."""
    try:
        files_str, _ = bash_command_get_output("git ls-tree -r --name-only HEAD")
        files = [f.strip() for f in files_str.strip().split('\n') if f.strip()]
    except Exception as e:
        log_warning(f"Could not get file list from git: {e}")
        return []
    
    return _filter_by_extensions(files, extensions)


def _get_modified_files(
    extensions: Optional[List[str]] = None,
    branch: str = "origin/main", 
    no_merge_base: bool = False
) -> List[str]:
    """Get modified files compared to the specified branch."""
    # Check for unstaged new files and warn about them
    unstaged_files = _get_unstaged_new_files()
    if unstaged_files:
        log_warning(
            f"Files not in working tree, so not included in diff: [{', '.join(unstaged_files)}]"
        )

    # Get the diff
    try:
        diff_out, _ = bash_command_get_output(
            f"git diff {'' if no_merge_base else '--merge-base'} {branch} --name-only"
        )
        files = [f.strip() for f in diff_out.strip().split('\n') if f.strip()]
    except Exception as e:
        log_warning(f"Could not get modified files from git: {e}")
        return []
    
    return _filter_by_extensions(files, extensions)


def _get_unstaged_new_files() -> List[str]:
    """Get list of files that are new but not staged."""
    try:
        status_out, _ = bash_command_get_output("git status --porcelain")
        new_unstaged_files = []
        for file in status_out.splitlines():
            if file.startswith("??"):
                new_unstaged_files.append(file[3:])
        return new_unstaged_files
    except Exception:
        return []


def _filter_by_extensions(files: List[str], extensions: Optional[List[str]]) -> List[str]:
    """Filter files by the specified extensions."""
    if not extensions:
        return files
    
    # Normalize extensions to ensure they start with a dot
    normalized_extensions = []
    for ext in extensions:
        if not ext.startswith('.'):
            ext = '.' + ext
        normalized_extensions.append(ext.lower())
    
    filtered_files = []
    for file in files:
        # Skip directories
        if os.path.isdir(file):
            continue
            
        # Check if file has one of the desired extensions
        file_ext = os.path.splitext(file)[1].lower()
        if file_ext in normalized_extensions:
            filtered_files.append(file)
    
    return filtered_files