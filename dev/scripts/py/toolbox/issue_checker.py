import re
import subprocess
import json
from pathlib import Path
from .cpp_linter import get_files_for_linter
from .helpers import log_info, log_warning, log_new_line

def get_issues_from_github():
    import os
    import shutil

    # Check if 'gh' is available
    if shutil.which('gh') is None:
        log_warning("'gh' CLI not found. Cannot run issue-checker without 'gh' or issue numbers.")
        return []

    # Get OWNER and REPO from git remote (support both SSH and HTTPS URLs)
    remote_url = subprocess.check_output(['git', 'remote', 'get-url', 'origin'], text=True).strip()
    m = re.match(r"(?:git@|https://)([^/:]+)[:/]+([^/]+)/([^/.]+)(?:\.git)?", remote_url)
    if not m:
        log_warning("Could not parse OWNER/REPO from git remote.")
        return []
    owner, repo = m.group(2), m.group(3)

    # Get current branch
    branch_name = subprocess.check_output(['git', 'rev-parse', '--abbrev-ref', 'HEAD'], text=True).strip()

    # Check for PR_NUMBER in environment (used in CI workflows)
    pr_number = os.environ.get("PR_NUMBER")
    if pr_number:
        log_info(f"Using PR_NUMBER from environment: {pr_number}")
    else:
        # If not set, try to get PR number associated with this branch using gh
        try:
            pr_number = subprocess.check_output(
                ['gh', 'pr', 'view', branch_name, '--json', 'number', '-q', '.number'],
                text=True
            ).strip()
            log_info(f"Found associated Pull Request number: {pr_number}")
        except subprocess.CalledProcessError:
            log_warning(f"No associated Pull Request found for branch: {branch_name}")
            return []

    if not pr_number:
        log_warning(f"No associated Pull Request found for branch: {branch_name}")
        return []

    query = f"""{{
        repository(owner: "{owner}", name: "{repo}") {{
            pullRequest(number: {pr_number}) {{
                closingIssuesReferences(first: 100) {{
                    nodes {{ number }}
                    }}
                }}
            }}
        }}"""
        
    # If WORKLOW_SCHED_TOKEN is set, use curl instead of gh
    workflow_token = os.environ.get("WORKLOW_SCHED_TOKEN")
    if workflow_token:
        # Use curl to query GitHub GraphQL API (for workflows)

        # Prepare the JSON payload
        json_query = json.dumps({"query": query})
        curl_cmd = [
            "curl", "-s",
            "-H", f"Authorization: bearer {workflow_token}",
            "-X", "POST",
            "-d", json_query,
            "https://api.github.com/graphql"
        ]
        try:
            curl_output = subprocess.check_output(curl_cmd, text=True)
            data = json.loads(curl_output)
            nodes = (
                data.get("data", {})
                    .get("repository", {})
                    .get("pullRequest", {})
                    .get("closingIssuesReferences", {})
                    .get("nodes", [])
            )
            return [str(node["number"]) for node in nodes if "number" in node]
        except Exception as e:
            log_warning(f"Error while fetching issue numbers via curl: {e}")
            return []
    else:
        gh_cmd = [
            'gh', 'api', 'graphql',
            '-f', f'query={query}'
        ]
        try:
            gh_output = subprocess.check_output(
                gh_cmd,
                text=True
            )
            data = json.loads(gh_output)
            nodes = (
                data.get("data", {})
                    .get("repository", {})
                    .get("pullRequest", {})
                    .get("closingIssuesReferences", {})
                    .get("nodes", [])
            )
            return [str(node["number"]) for node in nodes if "number" in node]
        except Exception as e:
            log_warning(f"Error while fetching issue numbers via gh api: {e}")
            return []

def issue_checker_impl(issues, branch: str = "origin/main", no_merge_base: bool = False, gh_token: str = ""):
    if(not issues):
        issues = get_issues_from_github()
        if not issues:
            return False

    valid_issue_numbers = []
    for num in issues:
        num_str = str(num).strip()
        if not num_str:
            continue
        if not num_str.isdigit() or int(num_str) <= 0:
            log_warning(f"Issue number '{num_str}' is not a positive integer. Skipping.")
            continue
        valid_issue_numbers.append(num_str)
    if not valid_issue_numbers:
        log_warning("No valid issue numbers provided.")
        return True

    # Match #number followed by a non-digit (whitespace, punctuation, or end of line)
    patterns = [re.compile(rf"#\b({re.escape(num)})(?!\d)") for num in valid_issue_numbers]
    files = get_files_for_linter(True, branch, no_merge_base).keys()
    found_any = False
    summary = {num: 0 for num in valid_issue_numbers}

    for file in files:
        with open(file, "r") as f:
            for i, line in enumerate(f, 1):
                for idx, pat in enumerate(patterns):
                    if pat.search(line):
                        log_warning(f"{file}:{i}: {line.strip()}")
                        summary[valid_issue_numbers[idx]] += 1
                        found_any = True
    log_new_line()
    log_info("Summary:")
    for num in valid_issue_numbers:
        log_info(f"#{num}: {summary[num]} occurrence(s)")
    return not found_any
