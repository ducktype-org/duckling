import re
import os
import json
import shutil
from .helpers import (
    BashCommandError,
    log_info, 
    log_warning, 
    log_new_line, 
    bash_command_get_output,
)

def get_open_issues_from_github():
    """
    Retrieves all open issue numbers from GitHub for the current repository.
    
    Returns:
        list[str]: List of open issue numbers as strings, empty list if none found or on error
    """
    # Check if 'gh' is available
    if shutil.which('gh') is None:
        log_warning("'gh' CLI not found. Cannot validate issue existence.")
        return []

    # Get OWNER and REPO from git remote (support both SSH and HTTPS URLs)
    try:
        remote_url, _ = bash_command_get_output('git remote get-url origin')
        remote_url = remote_url.strip()
    except BashCommandError as e:
        log_warning(f"Could not get git remote url: {e}")
        return []
    
    m = re.match(r"(?:git@|https://)([^/:]+)[:/]+([^/]+)/([^/.]+)(?:\.git)?", remote_url)
    if not m:
        log_warning("Could not parse OWNER/REPO from git remote.")
        return []
    owner, repo = m.group(2), m.group(3)

    # Query for open issues using GraphQL
    query = f"""{{
      repository(owner: "{owner}", name: "{repo}") {{
        issues(states: OPEN, first: 100) {{
          nodes {{ number }}
          pageInfo {{ hasNextPage endCursor }}
        }}
      }}
    }}"""

    gh_cmd = f"gh api graphql -f 'query={query}'"
    try:
        gh_output, _ = bash_command_get_output(gh_cmd)
        data = json.loads(gh_output)
        nodes = (
            data.get("data", {})
                .get("repository", {})
                .get("issues", {})
                .get("nodes", [])
        )
        issue_numbers = [str(node["number"]) for node in nodes if "number" in node]
        
        # Note: This only gets first 100 issues. For a more complete implementation,
        # we could handle pagination, but for TODO validation this should be sufficient.
        return issue_numbers
        
    except BashCommandError as e:
        log_warning(f"Error while fetching open issues via gh api: {e}")
        return []
    except json.JSONDecodeError as e:
        log_warning(f"Error parsing GitHub API response: {e}")
        return []

def todo_validate_impl(branch: str = "origin/main", no_merge_base: bool = False) -> bool:
    """
    Validates that all TODO/FIXME comments follow the required format with issue numbers.
    
    Returns True if all TODOs are properly formatted, False if any violations are found.
    """
    
    # Get valid open issue numbers from GitHub
    valid_issues = get_open_issues_from_github()
    if not valid_issues:
        log_warning("Could not fetch open issues from GitHub. Skipping issue existence validation.")
        valid_issues = []
    
    # Define strict patterns for TODO/FIXME comments in the required format
    # Format: @TODO #123 description or TODO #123 description
    todo_patterns = [
        re.compile(r'@TODO\s+#(\d+)\s+\S+', re.IGNORECASE),
        re.compile(r'@FIXME\s+#(\d+)\s+\S+', re.IGNORECASE),
        re.compile(r'\bTODO\s+#(\d+)\s+\S+', re.IGNORECASE),
        re.compile(r'\bFIXME\s+#(\d+)\s+\S+', re.IGNORECASE),
    ]
    
    # Patterns to detect any TODO/FIXME comment (for reporting violations)
    any_todo_patterns = [
        re.compile(r'@TODO\b', re.IGNORECASE),
        re.compile(r'@FIXME\b', re.IGNORECASE),
        re.compile(r'\bTODO\b', re.IGNORECASE),
        re.compile(r'\bFIXME\b', re.IGNORECASE),
    ]
    
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
    invalid_issue_count = 0
    
    log_info("Validating TODO/FIXME comments format...")
    
    for file in source_files:
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
                        elif valid_issues and issue_number not in valid_issues:
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