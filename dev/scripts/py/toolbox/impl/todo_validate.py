import re
import os
import json
import shutil
from typing import List, Optional
from .helpers import (
    BashCommandError,
    log_info, 
    log_warning, 
    log_new_line, 
    bash_command_get_output,
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
    if shutil.which('gh') is None:
        log_warning("'gh' CLI not found. Cannot validate issue existence.")
        return False

    # Get OWNER and REPO from git remote (support both SSH and HTTPS URLs)
    try:
        remote_url, _ = bash_command_get_output('git remote get-url origin')
        remote_url = remote_url.strip()
    except BashCommandError as e:
        log_warning(f"Could not get git remote url: {e}")
        return False
    
    m = re.match(r"(?:git@|https://)([^/:]+)[:/]+([^/]+)/([^/.]+)(?:\.git)?", remote_url)
    if not m:
        log_warning("Could not parse OWNER/REPO from git remote.")
        return False
    owner, repo = m.group(2), m.group(3)

    # Query for specific issue using GraphQL
    query = f"""{{
      repository(owner: "{owner}", name: "{repo}") {{
        issue(number: {issue_number}) {{
          number
          state
        }}
      }}
    }}"""

    gh_cmd = f"gh api graphql -f 'query={query}'"
    try:
        gh_output, _ = bash_command_get_output(gh_cmd)
        data = json.loads(gh_output)
        issue = (
            data.get("data", {})
                .get("repository", {})
                .get("issue")
        )
        
        if issue is None:
            return False  # Issue doesn't exist
            
        return issue.get("state") == "OPEN"
        
    except BashCommandError as e:
        log_warning(f"Error while checking issue #{issue_number} via gh api: {e}")
        return False
    except json.JSONDecodeError as e:
        log_warning(f"Error parsing GitHub API response: {e}")
        return False

def todo_validate_impl(branch: str = "origin/main", no_merge_base: bool = False) -> bool:
    """
    Validates that all TODO/FIXME comments follow the required format with issue numbers.
    
    Args:
        branch: Git branch to check against (default: origin/main)
        no_merge_base: If True, skip merge base calculation
    
    Returns:
        bool: True if all TODOs are properly formatted, False if any violations are found.
    """
    
    # Define strict patterns for TODO/FIXME comments in the required format
    # Format: @TODO #123 description or @FIXME #123 description (only @ prefixed)
    todo_patterns = [
        re.compile(r'@TODO\s+#(\d+)\s+\S+', re.IGNORECASE),
        re.compile(r'@FIXME\s+#(\d+)\s+\S+', re.IGNORECASE),
    ]
    
    # Patterns to detect any TODO/FIXME comment (for reporting violations)
    any_todo_patterns = [
        re.compile(r'@TODO\b', re.IGNORECASE),
        re.compile(r'@FIXME\b', re.IGNORECASE),
        re.compile(r'\bTODO\b', re.IGNORECASE),
        re.compile(r'\bFIXME\b', re.IGNORECASE),
    ]
    
    try:
        files = list_files_impl(only_modified=False)
    except Exception as e:
        log_warning(f"Could not get file list from git: {e}")
        return True
    
    # Check all files as requested - we don't store generated files in git
    violations_found = False
    violation_count = 0
    invalid_issue_count = 0
    
    log_info("Validating TODO/FIXME comments format...")
    
    for file in files:
        if os.path.isdir(file):
            continue
            
        try:
            with open(file, "r", encoding='utf-8', errors='ignore') as f:
                for line_num, line in enumerate(f, 1):
                    # Check if line contains any TODO/FIXME pattern
                    has_todo = any(pattern.search(line) for pattern in any_todo_patterns)
                    
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
                        elif issue_number and not check_issue_exists_and_open(issue_number):
                            log_warning(f"{file}:{line_num}: Issue #{issue_number} does not exist or is not open: {line.strip()}")
                            violations_found = True
                            invalid_issue_count += 1
                            
        except Exception as e:
            # Skip files that can't be read (binary files, permission issues, etc.)
            continue
    
    log_new_line()
    if violations_found:
        if violation_count > 0:
            log_warning(f"Found {violation_count} TODO/FIXME comment(s) without proper format")
        if invalid_issue_count > 0:
            log_warning(f"Found {invalid_issue_count} TODO/FIXME comment(s) with invalid or closed issue numbers")
        log_info("All TODO/FIXME comments must follow the format: @TODO #issue_number description")
        log_info("Example: // @TODO #0123 Implement this feature")
        log_info("The issue number must reference an open GitHub issue.")
    else:
        log_info("All TODO/FIXME comments are properly formatted with valid issue numbers")
    
    return not violations_found