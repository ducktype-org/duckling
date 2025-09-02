import re
import os
from .helpers import (
    BashCommandError,
    log_info, 
    log_warning, 
    log_new_line, 
    bash_command_get_output,
)

def todo_validate_impl(branch: str = "origin/main", no_merge_base: bool = False):
    """
    Validates that all TODO/FIXME comments follow the required format with issue numbers.
    
    Returns True if all TODOs are properly formatted, False if any violations are found.
    """
    
    # Define patterns for TODO/FIXME comments that need validation
    # Match patterns like @TODO, TODO, FIXME (case insensitive)
    todo_patterns = [
        re.compile(r'@\s*TODO\b', re.IGNORECASE),
        re.compile(r'@\s*FIXME\b', re.IGNORECASE),
        re.compile(r'\bTODO\b', re.IGNORECASE),
        re.compile(r'\bFIXME\b', re.IGNORECASE),
    ]
    
    # Pattern to match the required format: #followed by digits
    issue_pattern = re.compile(r'#\d+')
    
    try:
        files_str, _ = bash_command_get_output("git ls-tree -r --name-only HEAD")
        files = [f for f in files_str.strip().split('\n') if f]
    except Exception as e:
        log_warning(f"Could not get file list from git: {e}")
        return True
    
    # Filter to only check source files (exclude generated files, docs, etc.)
    source_extensions = {'.cpp', '.hpp', '.c', '.h', '.cc', '.cxx', '.hxx', '.py', '.cmake'}
    source_files = []
    for file in files:
        if os.path.isdir(file):
            continue
        _, ext = os.path.splitext(file)
        if ext.lower() in source_extensions or file.endswith('CMakeLists.txt'):
            source_files.append(file)
    
    violations_found = False
    violation_count = 0
    
    log_info("Validating TODO/FIXME comments format...")
    
    for file in source_files:
        try:
            with open(file, "r", encoding='utf-8', errors='ignore') as f:
                for line_num, line in enumerate(f, 1):
                    # Check if line contains any TODO/FIXME pattern
                    has_todo = any(pattern.search(line) for pattern in todo_patterns)
                    
                    if has_todo:
                        # Check if the line also contains an issue number
                        if not issue_pattern.search(line):
                            log_warning(f"{file}:{line_num}: {line.strip()}")
                            violations_found = True
                            violation_count += 1
        except Exception as e:
            # Skip files that can't be read (binary files, permission issues, etc.)
            continue
    
    log_new_line()
    if violations_found:
        log_warning(f"Found {violation_count} TODO/FIXME comment(s) without linked issue numbers")
        log_info("All TODO/FIXME comments must follow the format: @TODO #issue_number description")
        log_info("Example: // @TODO #0123 Implement this feature")
    else:
        log_info("All TODO/FIXME comments are properly formatted with issue numbers")
    
    return not violations_found