# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

import re
from .helpers import (
    get_input,
    log_good,
    log_info,
    log_warning,
    log_new_line,
)
from .license_checker import (
    HEADER_LINES,
    HeaderStatus,
    check_license_header,
    get_comment_prefix,
)
from .list_files import list_files_impl

PRAGMA_ONCE_PATTERN = re.compile(r"^\s*#\s*pragma\s+once\b")


def header_checker_impl(
    all: bool = False,
    branch: str = "origin/main",
    no_merge_base: bool = False,
    auto_fix: bool = False,
    no_fix: bool = False,
) -> bool:
    """
    Validates that:
      - All .hpp files (excluding .def.hpp) contain '#pragma once'
      - All .def.hpp files DO NOT contain '#pragma once'
    """
    files = list_files_impl(
        branch=branch,
        no_merge_base=no_merge_base,
        only_modified=not all,
        include_untracked=True,
        extensions=[".hpp"],
        filter_deleted=True,
    )

    missing_pragma_once: list[str] = []
    forbidden_pragma_once: list[str] = []
    unreadable_files: list[str] = []

    for file in files:
        try:
            has_pragma_once = _has_pragma_once(file)
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

    fixable_files = missing_pragma_once + forbidden_pragma_once
    if fixable_files and not no_fix:
        apply = auto_fix
        if not auto_fix:
            log_new_line()
            log_info(f"Can fix '#pragma once' in {len(fixable_files)} file(s).")
            response = get_input("Do you want to apply these fixes? [Y/n]")
            apply = response.lower() in ["y", "yes", ""]

        if apply:
            fixed_count = 0
            for file in missing_pragma_once:
                try:
                    _add_pragma_once(file)
                    fixed_count += 1
                except (OSError, UnicodeError) as e:
                    log_warning(f"Could not add '#pragma once' to {file}: {e}")

            for file in forbidden_pragma_once:
                try:
                    _remove_pragma_once(file)
                    fixed_count += 1
                except (OSError, UnicodeError) as e:
                    log_warning(f"Could not remove '#pragma once' from {file}: {e}")

            if fixed_count:
                log_good(f"Applied '#pragma once' fixes to {fixed_count} file(s)")

            missing_pragma_once = []
            forbidden_pragma_once = []
            for file in fixable_files:
                try:
                    has_pragma_once = _has_pragma_once(file)
                except OSError as e:
                    log_warning(f"Could not read {file}: {e}")
                    unreadable_files.append(file)
                    continue

                is_def_header = file.endswith(".def.hpp")
                if is_def_header and has_pragma_once:
                    forbidden_pragma_once.append(file)
                    log_warning(
                        f"{file}: Forbidden '#pragma once' found in .def.hpp file"
                    )
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
            log_info(f"Unreadable: {len(unreadable_files)} file(s)")
        return False

    log_good("All header '#pragma once' checks have passed")
    return True


def _has_pragma_once(file: str) -> bool:
    with open(file, "r", encoding="utf-8", errors="ignore") as f:
        return any(PRAGMA_ONCE_PATTERN.match(line) for line in f)


def _add_pragma_once(file: str) -> None:
    with open(file, "r", encoding="utf-8", newline="") as f:
        content = f.read()

    bom = "\ufeff" if content.startswith("\ufeff") else ""
    content = content.removeprefix(bom)
    newline = "\r\n" if "\r\n" in content else "\n"
    lines = content.splitlines(keepends=True)
    shebang_lines = int(_is_shebang(lines[0])) if lines else 0
    if check_license_header(file) == HeaderStatus.OK:
        header_end = shebang_lines + len(HEADER_LINES)
    else:
        # Preserve the leading comment preamble using the syntax supported by
        # the license checker for this file type.
        header_end = _leading_comment_end(lines, get_comment_prefix(file))

    header = lines[:header_end]
    body = lines[header_end:]
    if body and not body[0].strip():
        header.append(body.pop(0))

    prefix = "".join(header)
    if prefix and not prefix.endswith(newline * 2):
        prefix += newline if prefix.endswith(newline) else newline * 2
    content = bom + prefix + f"#pragma once{newline}{newline}" + "".join(body)

    with open(file, "w", encoding="utf-8", newline="") as f:
        f.write(content)


def _remove_pragma_once(file: str) -> None:
    with open(file, "r", encoding="utf-8", newline="") as f:
        lines = f.readlines()

    filtered_lines: list[str] = []
    skip_next_blank = False
    for line in lines:
        if PRAGMA_ONCE_PATTERN.match(line):
            skip_next_blank = True
            continue
        if skip_next_blank:
            skip_next_blank = False
            if not line.strip():
                continue
        filtered_lines.append(line)

    with open(file, "w", encoding="utf-8", newline="") as f:
        f.writelines(filtered_lines)


def _leading_comment_end(lines: list[str], comment_prefix: str | None) -> int:
    index = int(bool(lines) and _is_shebang(lines[0]))
    if comment_prefix is None:
        return index

    supports_block_comments = comment_prefix == "//"
    in_block_comment = False
    while index < len(lines):
        line = lines[index].lstrip()
        if not line.strip():
            break

        if in_block_comment:
            in_block_comment = "*/" not in line
            index += 1
            continue

        if line.startswith(comment_prefix):
            index += 1
            continue

        if supports_block_comments and line.startswith("/*"):
            in_block_comment = "*/" not in line[2:]
            index += 1
            continue

        break

    return index


def _is_shebang(line: str) -> bool:
    return line.startswith("#!") and not line[2:].lstrip().startswith("[")
