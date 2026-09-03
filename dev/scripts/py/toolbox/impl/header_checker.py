import re
from .helpers import (
    log_good,
    log_info,
    log_warning,
    log_new_line,
)
from .list_files import list_files_impl

PRAGMA_ONCE_PATTERN = re.compile(r"^\s*#\s*pragma\s+once\b")


def header_checker_impl(
    all_files: bool = False,
    branch: str = "origin/main",
    no_merge_base: bool = False
) -> bool:
    """
    Validates that:
      - All .hpp files (excluding .def.hpp) contain '#pragma once'
      - All .def.hpp files DO NOT contain '#pragma once'
    """
    files = list_files_impl(
        branch=branch,
        no_merge_base=no_merge_base,
        only_modified=not all_files,
        include_untracked=True,
        extensions=[".hpp"],
        filter_deleted=True,
    )

    missing_pragma_once: list[str] = []
    forbidden_pragma_once: list[str] = []
    unreadable_files: list[str] = []

    for file in files:
        has_pragma_once = False
        try:
            with open(file, "r", encoding="utf-8", errors="ignore") as f:
                for line in f:
                    if PRAGMA_ONCE_PATTERN.match(line):
                        has_pragma_once = True
                        break
        except OSError as e:
            log_warning(f"Could not read {file}: {e}")
            unreadable_files.append(file)
            continue

        is_def_header = file.endswith(".def.hpp")

        if is_def_header and has_pragma_once:
            forbidden_pragma_once.append(file)
            log_warning(f"{file}: Forbidden '#pragma once' found in .def.hpp file")
        elif not is_def_header and not has_pragma_once:
            missing_pragma_once.append(file)
            log_warning(f"{file}: Missing '#pragma once'")

    if missing_pragma_once or forbidden_pragma_once or unreadable_files:
        log_new_line()
        log_info("Header check summary:")
        if missing_pragma_once:
            log_info(f"Missing '#pragma once': {len(missing_pragma_once)} file(s)")
        if forbidden_pragma_once:
            log_info(
                f"Forbidden '#pragma once' in .def.hpp: {len(forbidden_pragma_once)} file(s)"
            )
        if unreadable_files:
            log_info(f"Unreadable file(s): {len(unreadable_files)} file(s)")
        return False

    log_good("All header '#pragma once' checks have passed")
    return True
