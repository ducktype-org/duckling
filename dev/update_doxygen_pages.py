#!/usr/bin/env python3
"""
Script to scan repository and add/update Doxygen page indicators in markdown files.
"""

import os
import re
import glob
from pathlib import Path
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
        # If we encounter a \page or @page directive, extract the human-readable name from it
        elif line.startswith('\\page') or line.startswith('@page'):
            parts = line.split(' ', 2)
            if len(parts) >= 3:
                return parts[2]  # The human-readable name
    return None


def generate_page_name(file_path: str, base_dir: str) -> str:
    """Generate Doxygen page name from file path."""
    # Get relative path from base directory
    rel_path = os.path.relpath(file_path, base_dir)
    # Remove the filename (readme.md or README.md)
    dir_path = os.path.dirname(rel_path)
    
    if not dir_path or dir_path == '.':
        return "main-readme"
    
    # Replace path separators with dashes and clean up
    page_name = dir_path.replace('/', '-').replace('\\', '-')
    # Remove any leading/trailing dashes
    page_name = page_name.strip('-')
    
    # Add readme suffix if not already present
    if not page_name.endswith('-readme'):
        page_name += '-readme'
    
    return page_name


def find_subdirectory_readmes(dir_path: str) -> List[str]:
    """Find all readme files in subdirectories recursively."""
    subdirs = []
    
    for item in os.listdir(dir_path):
        item_path = os.path.join(dir_path, item)
        if os.path.isdir(item_path):
            # Look for readme files in this subdirectory
            readme_patterns = ['readme.md', 'README.md', 'Readme.md']
            has_readme = False
            
            for pattern in readme_patterns:
                readme_path = os.path.join(item_path, pattern)
                if os.path.exists(readme_path):
                    subdirs.append(item)
                    has_readme = True
                    break
            
            # If this subdirectory doesn't have a readme, look deeper
            if not has_readme:
                try:
                    nested_subdirs = find_nested_readmes(item_path, item)
                    subdirs.extend(nested_subdirs)
                except:
                    # Skip if we can't access the directory
                    pass
    
    return sorted(subdirs)


def find_nested_readmes(dir_path: str, prefix: str) -> List[str]:
    """Find readme files in nested subdirectories."""
    nested_dirs = []
    
    for item in os.listdir(dir_path):
        item_path = os.path.join(dir_path, item)
        if os.path.isdir(item_path):
            # Look for readme files in this nested subdirectory
            readme_patterns = ['readme.md', 'README.md', 'Readme.md']
            
            for pattern in readme_patterns:
                readme_path = os.path.join(item_path, pattern)
                if os.path.exists(readme_path):
                    nested_name = f"{prefix}/{item}"
                    nested_dirs.append(nested_name)
                    break
    
    return nested_dirs


def generate_subpage_name(parent_page: str, subdir: str) -> str:
    """Generate subpage name from parent page and subdirectory."""
    # Handle nested subdirectories (e.g., "src/base")
    subdir_path = subdir.replace('/', '-')
    
    if parent_page == "mainpage":
        return f"{subdir_path}-readme"
    elif parent_page == "main-readme":
        return f"{subdir_path}-readme"
    else:
        # Remove -readme suffix from parent if present, add subdir, then add -readme
        parent_base = parent_page.replace('-readme', '')
        return f"{parent_base}-{subdir_path}-readme"


def update_markdown_file(file_path: str, base_dir: str) -> Tuple[bool, Optional[str]]:
    """
    Update a markdown file with proper Doxygen page indicator.
    Returns (success, missing_header_info)
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
            page_name = "mainpage"  # For generating subpage names
        else:
            # Extract first header
            first_header = extract_first_header(content)
            if not first_header:
                return False, f"No header found in {file_path}"
            
            # Generate page name
            page_name = generate_page_name(file_path, base_dir)
            
            # Create the page directive
            page_directive = f"\\page {page_name} {first_header}"
        
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
        if is_index_file:
            if mainpage_line != -1:
                # Replace existing mainpage directive
                lines[mainpage_line] = page_directive
            elif page_directive_line != -1:
                # Replace existing page directive with mainpage
                lines[page_directive_line] = page_directive
            else:
                # Insert at the beginning
                lines.insert(0, page_directive)
                lines.insert(1, "")
        else:
            if page_directive_line != -1 or mainpage_line != -1:
                # Replace existing page directive
                target_line = page_directive_line if page_directive_line != -1 else mainpage_line
                lines[target_line] = page_directive
            elif first_header_line != -1:
                # Insert before first header
                lines.insert(first_header_line, page_directive)
                lines.insert(first_header_line + 1, "")  # Add empty line
            else:
                # Insert at the beginning
                lines.insert(0, page_directive)
                lines.insert(1, "")
        
        # Find and update subpages section
        dir_path = os.path.dirname(file_path)
        subdirs = find_subdirectory_readmes(dir_path)
        
        if subdirs:
            subpage_lines = []
            for subdir in subdirs:
                subpage_name = generate_subpage_name(page_name, subdir)
                # Create hybrid links that work in both Markdown and Doxygen
                if '/' in subdir:
                    # For nested paths like "src/base"
                    md_link = f"{subdir}/readme.md"
                else:
                    # For direct subdirectories
                    md_link = f"{subdir}/readme.md"
                
                subpage_lines.append(f"- [{subdir}]({md_link}) (\\subpage {subpage_name})")
            
            # Find existing subpages section
            subpage_start = -1
            subpage_end = -1
            
            for i, line in enumerate(lines):
                if '\\subpage' in line:
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
        
        # Write back to file
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write('\n'.join(lines))
        
        return True, None
        
    except Exception as e:
        return False, f"Error processing {file_path}: {str(e)}"


def main():
    """Main function to process all markdown files."""
    # Get the base directory (where script is run from)
    base_dir = os.getcwd()
    
    print(f"Scanning repository from: {base_dir}")
    
    # Find all markdown files
    readme_files = []
    for pattern in ['**/readme.md', '**/README.md', '**/Readme.md', '**/Index.md']:
        readme_files.extend(glob.glob(pattern, recursive=True))
    
    # Convert to absolute paths and remove duplicates
    readme_files = list(set(os.path.abspath(f) for f in readme_files))
    
    # Filter out build directories and external dependencies
    filtered_files = []
    exclude_patterns = ['build/', 'build-', '_deps/', '/deps/', 'docs/config']
    
    for file_path in readme_files:
        rel_path = os.path.relpath(file_path, base_dir)
        should_exclude = False
        
        for pattern in exclude_patterns:
            if pattern in rel_path:
                should_exclude = True
                break
        
        if not should_exclude:
            filtered_files.append(file_path)
    
    readme_files = filtered_files
    print(f"Found {len(readme_files)} markdown files (after filtering)")
    
    successful_updates = []
    files_without_headers = []
    
    for file_path in sorted(readme_files):
        print(f"Processing: {os.path.relpath(file_path, base_dir)}")
        success, error_msg = update_markdown_file(file_path, base_dir)
        
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
