import re
from pathlib import Path
from .cpp_linter import get_files_for_linter
from .helpers import log_info, log_warning, log_new_line

def issue_checker_impl(issues, branch: str = "origin/main", no_merge_base: bool = False):
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
        return False

    # Match #number followed by a non-digit (whitespace, punctuation, or end of line)
    patterns = [re.compile(rf"#\b({re.escape(num)})(?!\d)") for num in valid_issue_numbers]
    files = get_files_for_linter(True, branch, no_merge_base).keys()
    found_any = False
    summary = {num: 0 for num in valid_issue_numbers}

    for file in files:
        if not (file.endswith(".cpp") or file.endswith(".hpp")):
            continue
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
    return found_any
