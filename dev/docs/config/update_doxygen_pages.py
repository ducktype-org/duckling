#!/usr/bin/env python3
# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

"""
Script to scan repository and add/update lists of direct descendant markdown files as links in each README.md or markdown file.
This is intended to help navigation between related documentation pages.
"""

import os
import re
import glob
import fnmatch
from typing import List, Optional


def extract_first_header(content: str) -> Optional[str]:
    """Extract the first markdown header from content."""
    for line in content.split("\n"):
        line = line.strip()
        if line.startswith("#"):
            header = line.lstrip("#").strip()
            if header:
                return header
    return None


def update_markdown_file(file_path: str, base_dir: str, links: List[str]) -> tuple[bool, bool]:
    """
    Update a markdown file by adding/updating a list of direct descendant links at the end.
    file_path: Absolute path to the markdown file
    base_dir: Base directory as absolute path to resolve relative paths
    links: List of direct descendant readme absolute paths
    
    Returns: (success, has_h1_header)
    """
    try:
        current_dir = os.path.dirname(file_path)
        with open(file_path, "r", encoding="utf-8") as f:
            content = f.read()
        lines = content.split("\n")
        
        # Check if file has H1 header
        has_h1_header = extract_first_header(content) is not None

        # Find existing descendant link lists pattern
        subpage_start = -1
        subpage_end = -1
        insert_position = len(lines)  # Default to end of file
        
        for i, line in enumerate(lines):
            if re.match(r"^[\*\-]\s+\[.*?\]\(.*?\.md\)$", line):
                if subpage_start == -1:
                    subpage_start = i
                    # Check if there's an empty line before the pattern
                    if i > 0 and lines[i-1].strip() == "":
                        subpage_start = i - 1
                subpage_end = i
        
        # Prepare new descendant links
        subpage_lines = []
        for absolute_readme_path in links:
            relative_readme_path = os.path.relpath(absolute_readme_path, current_dir)
            desc = (
                os.path.dirname(relative_readme_path)
                if os.path.basename(relative_readme_path).lower() == "readme.md"
                else os.path.basename(relative_readme_path)[:-3]
            )
            subpage_lines.append(f"* [{desc}](./{relative_readme_path})")
        
        if subpage_lines:
            if subpage_start != -1:
                # Replace existing pattern in place
                # Check if we need an empty line before
                need_empty_before = subpage_start > 0 and lines[subpage_start-1].strip() != ""
                # Check if there's an empty line after the existing pattern
                has_empty_after = (subpage_end + 1 < len(lines) and 
                                 lines[subpage_end + 1].strip() == "")
                
                replacement = []
                if need_empty_before:
                    replacement.append("")
                replacement.extend(subpage_lines)
                if not has_empty_after and subpage_end + 1 < len(lines):
                    replacement.append("")
                
                lines[subpage_start:subpage_end + 1] = replacement
            else:
                # Add at the end, ensuring proper spacing
                # Remove trailing empty lines
                while lines and lines[-1].strip() == "":
                    lines.pop()
                
                if lines and lines[-1].strip():
                    lines.append("")
                lines.extend(subpage_lines)

        # Ensure proper file ending - add newline only if file doesn't already end with one
        content_to_write = "\n".join(lines)
        if content_to_write and not content_to_write.endswith("\n"):
            content_to_write += "\n"
        
        with open(file_path, "w", encoding="utf-8") as f:
            f.write(content_to_write)
        return True, has_h1_header
    except Exception:
        return False, False


def get_direct_descendants(readme_path: str, all_readme_paths: List[str]) -> List[str]:
    """
    Get all direct descendant markdown files of the given directory.
    Only returns files that are immediate children, not deeper descendants.
    """
    directory_path = os.path.dirname(readme_path)
    descendants_readmes = [
        path
        for path in all_readme_paths
        if path.startswith(directory_path) and os.path.dirname(path) != directory_path
    ]
    direct_descendants = []
    for candidate_readme in sorted(descendants_readmes):
        candidate_dir = os.path.dirname(candidate_readme)
        for other_readme in descendants_readmes:
            other_dir = os.path.dirname(other_readme)
            if candidate_dir.startswith(other_dir) and other_dir != candidate_dir:
                break
        else:
            direct_descendants.append(candidate_readme)
    return direct_descendants


def main():
    """
    Main function to process all markdown files and add/update descendant links.
    """
    base_dir = os.getcwd()
    print(f"Scanning repository from: {base_dir}")
    readme_files = []
    for pattern in ["**/*.md"]:
        readme_files.extend(glob.glob(pattern, recursive=True))
    readme_files = list(set(os.path.abspath(f) for f in readme_files))
    exclude_patterns = [
        "readme.md",
        "build/*",
        "build*/*",
        "_deps/*",
        "deps/*",
        "docs/config/*",
        ".venv/*",
        ".cache/*",
    ]

    def is_excluded(path):
        return any(fnmatch.fnmatch(path, pattern) for pattern in exclude_patterns)

    filtered_files = [
        file_path for file_path in readme_files if not is_excluded(os.path.relpath(file_path, base_dir))
    ]
    readme_files = sorted(filtered_files)
    print(f"Found {len(readme_files)} markdown files (after filtering)")
    successful_updates = []
    files_without_headers = []
    for file_path in readme_files:
        print(f"Processing: {os.path.relpath(file_path, base_dir)}")
        links = get_direct_descendants(file_path, readme_files)
        success, has_h1_header = update_markdown_file(file_path, base_dir, links)
        if success:
            successful_updates.append(file_path)
            print(f"  ✓ Updated successfully")
            if not has_h1_header:
                files_without_headers.append(file_path)
                print(f"  ⚠ Warning: No H1 header found")
        else:
            print(f"  ✗ Error updating {file_path}")
    print(f"\n=== SUMMARY ===")
    print(f"Successfully updated: {len(successful_updates)} files")
    if files_without_headers:
        print(f"Files without H1 headers: {len(files_without_headers)} files")
        print(f"\n=== FILES WITHOUT H1 HEADERS ===")
        for file_path in files_without_headers:
            rel_path = os.path.relpath(file_path, base_dir)
            print(f"- {rel_path}")


if __name__ == "__main__":
    main()
