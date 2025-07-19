#!/usr/bin/env python3
"""
Script to scan repository and add/update Doxygen page indicators in markdown files.
"""

import os
import re
import glob
import fnmatch

from typing import List, Tuple, Optional


def extract_first_header(content: str) -> Optional[str]:
    """Extract the first markdown header from content."""
    lines = content.split('\n')
    for line in lines:
        line = line.strip()
        if line.startswith('#'):
            # Remove leading # and whitespace
            header = line.lstrip('#').strip()
            if header:
                return header
    return None



def update_markdown_file(file_path: str, base_dir: str, links: str) -> Tuple[bool, Optional[str]]:
    """
    Update a markdown file with proper Doxygen page indicator.
    Returns (success, missing_header_info)\
    
    file_path: Absolute ath to the markdown file
    base_dir: Base directory as absolute path to resolve relative paths
    links: List of direct descendant readme absolute paths
    """
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Check if this is the main Index.md file
        filename = os.path.basename(file_path)
        is_index_file = filename.lower() == 'index.md' and os.path.dirname(file_path) == base_dir
        
        if is_index_file:
            # For Index.md, use \mainpage
            page_directive = "\\mainpage"
        
        lines = content.split('\n')
        
        # Find existing page directive or first header
        page_directive_line = -1
        mainpage_line = -1
        first_header_line = -1
        
        for i, line in enumerate(lines):
            if line.strip().startswith('\\mainpage'):
                mainpage_line = i
            elif line.strip().startswith('\\page') or line.strip().startswith('@page'):
                page_directive_line = i
            elif line.strip().startswith('#') and first_header_line == -1:
                first_header_line = i
        
        # Update or insert page directive
        if mainpage_line != -1:
            lines = lines[mainpage_line:]
        elif page_directive_line != -1:
            lines = lines[page_directive_line:]
        elif first_header_line != -1:
            lines = ["", ""] + lines[first_header_line:]
        else:
            # Insert at the beginning
            lines = ["", ""] + lines
        
        if is_index_file:
            lines[0] = page_directive
        else:
            lines = lines[2:]
        
        
        if links:
            subpage_lines = []
            for absolute_readme_path in links:
                relative_readme_path = os.path.relpath(absolute_readme_path, os.path.dirname(file_path))
                if os.path.basename(relative_readme_path).lower() == 'readme.md':
                    desc = os.path.dirname(relative_readme_path)
                else:
                    desc = os.path.basename(relative_readme_path)[:-3]
                
                subpage_lines.append(f"- [{desc}](./{relative_readme_path})")
            
            # Find existing subpages section
            subpage_start = -1
            subpage_end = -1
            
            for i, line in enumerate(lines):
                if line.strip().startswith('- ['):
                    if subpage_start == -1:
                        subpage_start = i
                    subpage_end = i
            
            if subpage_start != -1:
                # Replace existing subpages
                lines[subpage_start:subpage_end + 1] = subpage_lines
            else:
                # Add subpages at the end
                if lines and lines[-1].strip():
                    lines.append("")
                lines.extend(subpage_lines)
            
            for i in reversed(range(len(lines))):
                if lines[i].strip() == "":
                    lines.pop(i)
                else:
                    break

            lines.append("")  # Ensure there's a trailing newline
            
        # Write back to file
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write('\n'.join(lines))
        
        return True, None
        
    except Exception as e:
        return False, f"Error processing {file_path}: {str(e)}"


def get_direct_descendants(readme_path: str, all_readme_paths: List[str]) -> List[str]:
    """
    Get all paths from relative list that are direct descendants of the given directory.
    Like:
    We are in x:

    w
    w/z
    x/y/z
    x/y/z/d

    only x/y/z is a direct descendant of x.
    x/y/z/d is not a direct descendant of x, because it is a descendant of x/y/z.
    """
    directory_path = os.path.dirname(readme_path)
    descendants_readmes = [path for path in all_readme_paths if path.startswith(directory_path) and os.path.dirname(path) != directory_path]

    direct_descendants = []
    for candidate_readme in sorted(descendants_readmes):
        candidate_dir = os.path.dirname(candidate_readme)
        # Check if has some prefix
        for other_readme in descendants_readmes:
            other_dir = os.path.dirname(other_readme)
            
            if candidate_dir.startswith(other_dir) and other_dir != candidate_dir:
                break
        else:
            direct_descendants.append(candidate_readme)
            
    return direct_descendants


def main():
    """Main function to process all markdown files."""
    # Get the base directory (where script is run from)
    base_dir = os.getcwd()
    
    print(f"Scanning repository from: {base_dir}")
    
    # Find all markdown files
    readme_files = []
    for pattern in ['**/*.md']:
        readme_files.extend(glob.glob(pattern, recursive=True))
    
    # Convert to absolute paths and remove duplicates
    readme_files = list(set(os.path.abspath(f) for f in readme_files))
    
    # Filter out build directories and external dependencies
    filtered_files = []
    exclude_patterns = ['readme.md', 'build/*', 'build*/*', '_deps/*', 'deps/*', 'docs/config/*', '.venv/*', '.cache/*']
    
    def is_excluded(path):
        return any(fnmatch.fnmatch(path, pattern) for pattern in exclude_patterns)

    for file_path in readme_files:
        rel_path = os.path.relpath(file_path, base_dir)
        
        if is_excluded(rel_path):
            continue
        
        filtered_files.append(file_path)
    
    readme_files = filtered_files
    print(f"Found {len(readme_files)} markdown files (after filtering)")
    
    successful_updates = []
    files_without_headers = []
    
    for file_path in sorted(readme_files):
        print(f"Processing: {os.path.relpath(file_path, base_dir)}")
        links = get_direct_descendants(file_path, readme_files)
        success, error_msg = update_markdown_file(file_path, base_dir, links)
        
        if success:
            successful_updates.append(file_path)
            print(f"  ✓ Updated successfully")
        else:
            files_without_headers.append((file_path, error_msg))
            print(f"  ✗ {error_msg}")
    
    print(f"\n=== SUMMARY ===")
    print(f"Successfully updated: {len(successful_updates)} files")
    print(f"Files with issues: {len(files_without_headers)} files")
    
    if files_without_headers:
        print(f"\n=== FILES WITHOUT HEADERS OR WITH ERRORS ===")
        for file_path, error in files_without_headers:
            rel_path = os.path.relpath(file_path, base_dir)
            print(f"- {rel_path}: {error}")


if __name__ == "__main__":
    main()
