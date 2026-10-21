# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

"""
Every source file starts with the license header, written in the comment
syntax of its language. Only a shebang line (and a UTF-8 BOM) may come before it:

    // Copyright 2026 DuckType LLC
    //
    // This file is part of the Duckling project, licensed under the DuckType
    // Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
    // of this repository or https://ducktype.org/licenses/DTCL-1.0
"""

import datetime
import os
import re
from enum import Enum

from .helpers import (
    get_input,
    log_good,
    log_info,
    log_new_line,
    log_warning,
)
from .list_files import list_files_impl

UTF8_BOM = "\ufeff"

# File suffix -> the line comment prefix the header is written with
COMMENT_PREFIXES: dict[str, str] = {
    ".c": "//",
    ".h": "//",
    ".cpp": "//",
    ".hpp": "//",
    ".cpp.in": "//",
    ".hpp.in": "//",
    ".rs": "//",
    ".py": "#",
}

# Third-party sources, which keep their own license. Paths relative to `dev/`.
EXCLUDED_FILES: set[str] = {
    "src/playground/llvm_example_playground.cpp",
}

HEADER_LINES: list[str] = [
    "Copyright {year} DuckType LLC",
    "",
    "This file is part of the Duckling project, licensed under the DuckType",
    "Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root",
    "of this repository or https://ducktype.org/licenses/DTCL-1.0",
]

# Any year is accepted: a file keeps the year it was first published in
_YEAR_PATTERN = r"\d{4}"

# How deep into a file to look for a header that is there but wrong
_COPYRIGHT_SEARCH_LINES = 10


def get_comment_prefix(path: str) -> str | None:
    """The comment prefix of the header in `path`, None if it needs no header."""
    for suffix, prefix in COMMENT_PREFIXES.items():
        if path.endswith(suffix):
            return prefix
    return None


def make_header(prefix: str, year: int) -> list[str]:
    """The license header lines, commented out with `prefix`."""
    return [
        f"{prefix} {line.format(year=year)}" if line else prefix
        for line in HEADER_LINES
    ]


def _header_patterns(prefix: str) -> list[re.Pattern]:
    """One full-line regex per header line, matching any copyright year."""
    patterns = []
    for line in HEADER_LINES:
        if not line:
            patterns.append(re.compile(re.escape(prefix)))
            continue
        escaped = re.escape(f"{prefix} {line}").replace(
            re.escape("{year}"), _YEAR_PATTERN
        )
        patterns.append(re.compile(escaped))
    return patterns


def _starts_with_shebang(text: str) -> bool:
    """
    Whether `text` starts with a shebang. Rust's inner attributes, like
    `#![allow(dead_code)]`, start with `#!` too, but they are code, so the header
    goes above them. Just like rustc, a `#!` followed by a `[` is not a shebang.
    """
    return text.startswith("#!") and not text[2:].lstrip().startswith("[")


def _split_preamble(lines: list[str]) -> tuple[list[str], list[str]]:
    """Split off the lines that may precede the header (a shebang), keeping the rest."""
    if lines and _starts_with_shebang(lines[0]):
        return lines[:1], lines[1:]
    return [], lines


class HeaderStatus(Enum):
    """The state of the license header in a file, valued with what to report."""

    OK = "has the license header"
    # Only this one can be fixed automatically, by prepending the header
    MISSING = "missing license header"
    # A file that mentions a copyright near its top has a wrong or a foreign
    # header, which only a human can sort out
    FOREIGN = (
        "malformed or foreign license header, "
        "cannot add the header automatically, fix it by hand"
    )
    UNREADABLE = (
        "cannot be read as UTF-8 text, "
        "cannot add the header automatically, fix it by hand"
    )


def check_license_header(path: str) -> HeaderStatus:
    """The state of the header in `path`, which has to start it, after an optional shebang."""
    prefix = get_comment_prefix(path)
    if prefix is None:
        return HeaderStatus.OK

    try:
        with open(path, "r", encoding="utf-8-sig") as f:
            lines = f.read().splitlines()
    except (OSError, UnicodeDecodeError):
        return HeaderStatus.UNREADABLE

    _, body = _split_preamble(lines)
    patterns = _header_patterns(prefix)
    if len(body) >= len(patterns) and all(
        pattern.fullmatch(line.rstrip()) for pattern, line in zip(patterns, body)
    ):
        return HeaderStatus.OK
    if any("copyright" in line.lower() for line in lines[:_COPYRIGHT_SEARCH_LINES]):
        return HeaderStatus.FOREIGN
    return HeaderStatus.MISSING


def add_license_header(path: str) -> None:
    """Insert the license header at the top of `path`, after an optional shebang."""
    prefix = get_comment_prefix(path)
    assert prefix is not None

    with open(path, "r", encoding="utf-8", newline="") as f:
        content = f.read()

    bom = UTF8_BOM if content.startswith(UTF8_BOM) else ""
    content = content.removeprefix(bom)
    newline = "\r\n" if "\r\n" in content else "\n"

    preamble = ""
    if _starts_with_shebang(content):
        end = content.find("\n") + 1 or len(content)
        preamble, content = content[:end], content[end:]
        if not preamble.endswith("\n"):
            preamble += newline

    header = newline.join(make_header(prefix, datetime.date.today().year)) + newline
    separator = newline if content.strip() else ""

    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(bom + preamble + header + separator + content)


def get_files_to_check(all: bool, branch: str, no_merge_base: bool) -> list[str]:
    """The files that need a license header: all of them, or just the diff.

    Untracked files are included in both modes (gitignored ones are not).
    """
    files: list[str] = list_files_impl(
        only_modified=not all,
        branch=branch,
        no_merge_base=no_merge_base,
        include_untracked=True,
    )  # type: ignore
    return sorted(
        file
        for file in set(files)
        # The diff also lists files the branch deleted
        if os.path.isfile(file)
        and file not in EXCLUDED_FILES
        and get_comment_prefix(file) is not None
    )


def license_checker_impl(
    all: bool = False,
    branch: str = "origin/main",
    no_merge_base: bool = False,
    auto_fix: bool = False,
    no_fix: bool = False,
) -> bool:
    """
    Check that every source file starts with the license header.

    Args:
        all: If True, check all files (tracked and untracked), otherwise only
            the ones in the diff
        branch: The branch the diff is made against
        no_merge_base: If True, diff against the tip of `branch`, not the merge base
        auto_fix: If True, add the missing headers without asking
        no_fix: If True, only report the missing headers

    Returns:
        bool: True if every checked file has the license header.
    """
    files = get_files_to_check(all, branch, no_merge_base)
    if not files:
        log_info("No source files to check for the license header. Nothing to check.")
        return True

    statuses = {file: check_license_header(file) for file in files}
    failed = [file for file, status in statuses.items() if status != HeaderStatus.OK]
    for file in failed:
        log_warning(f"{file}: {statuses[file].value}")

    fixable = [file for file in failed if statuses[file] == HeaderStatus.MISSING]
    if fixable and not no_fix:
        apply = auto_fix
        if not auto_fix:
            log_new_line()
            log_info(f"Can add the license header to {len(fixable)} file(s).")
            response = get_input("Do you want to add it? [Y/n]")
            apply = response.lower() in ["y", "yes", ""]

        if apply:
            for file in fixable:
                add_license_header(file)
            log_good(f"License header added to {len(fixable)} file(s)")
            # Only the fixed files have changed, so only they need a re-check
            failed = [
                file
                for file in failed
                if file not in fixable or check_license_header(file) != HeaderStatus.OK
            ]

    if failed:
        log_warning(
            f"{len(failed)} of {len(files)} file(s) have no valid license header"
        )
        log_info(
            "Every source file has to start with the header in its comment syntax:"
        )
        year = datetime.date.today().year
        for prefix in sorted({get_comment_prefix(file) or "" for file in failed}):
            log_new_line()
            for line in make_header(prefix, year):
                log_info(f"    {line}")
        log_new_line()
        log_info("Only a shebang line may precede it.")
        return False

    log_good(f"All {len(files)} checked file(s) have the license header")
    return True
