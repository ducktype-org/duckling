#!/usr/bin/env python3
# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

"""
Prepends the license notice from `file_notice` (repository root) to every
C/C++, Rust and Python source file tracked by git.

The notice is written as plain, non-documentation comments (`//` and `#`), so
Doxygen ignores it regardless of the doc-comment style a file uses
(`/** */`, `/*! */`, `///`, `//!`, `##`) and it never becomes the
documentation of a file, namespace or entity that follows it.

Files are skipped when they:
  * already contain the notice,
  * carry a foreign copyright/license header (third-party code),
  * are not regular files (e.g. symlinks).

Usage:
  add_file_notice.py            # modify files
  add_file_notice.py --dry-run  # only list what would change
  add_file_notice.py --check    # exit 1 if any file lacks the notice (CI)
"""

import argparse
import re
import subprocess
import sys
import textwrap
from pathlib import Path

WRAP_WIDTH = 80
HEADER_SCAN_LINES = 30

CPP_EXTENSIONS = {
    ".c", ".cc", ".cpp", ".cxx", ".c++", ".cppm", ".ixx",
    ".h", ".hh", ".hpp", ".hxx", ".h++", ".ipp", ".tpp", ".inl",
}
RUST_EXTENSIONS = {".rs"}
PYTHON_EXTENSIONS = {".py", ".pyi", ".pyw", ".pyx", ".pxd"}

# Extension of the file a `*.in` template (CMake configure_file) produces.
TEMPLATE_SUFFIX = ".in"

FOREIGN_LICENSE_RE = re.compile(r"SPDX-License-Identifier|Copyright|\(c\)|©", re.IGNORECASE)
PYTHON_CODING_RE = re.compile(r"^[ \t\f]*#.*?coding[:=]")


def repo_root() -> Path:
    out = subprocess.run(
        ["git", "rev-parse", "--show-toplevel"], check=True, capture_output=True, text=True
    )
    return Path(out.stdout.strip())


def tracked_files(root: Path) -> list[Path]:
    out = subprocess.run(
        ["git", "ls-files", "-z"], cwd=root, check=True, capture_output=True, text=True
    )
    return [root / p for p in out.stdout.split("\0") if p]


def comment_prefix(path: Path) -> str | None:
    suffix = path.suffix.lower()
    if suffix == TEMPLATE_SUFFIX:
        suffix = Path(path.stem).suffix.lower()
    if suffix in CPP_EXTENSIONS or suffix in RUST_EXTENSIONS:
        return "//"
    if suffix in PYTHON_EXTENSIONS:
        return "#"
    return None


def render_notice(notice: str, prefix: str, newline: str) -> str:
    width = WRAP_WIDTH - len(prefix) - 1
    lines: list[str] = []
    for paragraph in notice.strip().split("\n\n"):
        if lines:
            lines.append(prefix)
        text = " ".join(paragraph.split())
        wrapped = textwrap.wrap(text, width, break_long_words=False, break_on_hyphens=False)
        lines.extend(f"{prefix} {line}" for line in wrapped)
    return newline.join(lines) + newline


def header_comment_lines(lines: list[str], prefix: str) -> list[str]:
    comment_starts = ("//", "/*", "*") if prefix == "//" else ("#",)
    head = (line.strip() for line in lines[:HEADER_SCAN_LINES])
    return [line for line in head if line.startswith(comment_starts)]


def insertion_index(lines: list[str], prefix: str) -> int:
    """Index of the first line the notice may precede."""
    if prefix != "#":
        return 0
    index = 0
    # A shebang must stay on the first line, an encoding declaration on the first two.
    if lines and lines[0].startswith("#!"):
        index = 1
    if len(lines) > index and index < 2 and PYTHON_CODING_RE.match(lines[index]):
        index += 1
    return index


def process(path: Path, notice: str, first_notice_line: str) -> str:
    """Returns 'added', 'present', 'foreign' or 'skipped' and updates `path` in place if 'added'."""
    prefix = comment_prefix(path)
    if prefix is None or path.is_symlink() or not path.is_file():
        return "skipped"

    raw = path.read_bytes()
    bom = b"\xef\xbb\xbf" if raw.startswith(b"\xef\xbb\xbf") else b""
    text = raw[len(bom):].decode("utf-8")
    newline = "\r\n" if "\r\n" in text else "\n"
    lines = text.splitlines(keepends=True)

    comments = header_comment_lines(lines, prefix)
    if any(first_notice_line in line for line in comments):
        return "present"
    if any(FOREIGN_LICENSE_RE.search(line) for line in comments):
        return "foreign"

    index = insertion_index(lines, prefix)
    before = "".join(lines[:index])
    after = "".join(lines[index:])
    if before and not before.endswith(("\n", "\r")):
        before += newline

    block = render_notice(notice, prefix, newline)
    if after.strip():
        block += newline
        after = after.lstrip("\r\n")

    path.write_bytes(bom + (before + block + after).encode("utf-8"))
    return "added"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--dry-run", action="store_true", help="only list files that would be modified")
    mode.add_argument("--check", action="store_true", help="exit with 1 if any file lacks the notice")
    args = parser.parse_args()

    root = repo_root()
    notice = (root / "file_notice").read_text(encoding="utf-8")
    first_notice_line = notice.strip().splitlines()[0].strip()

    results: dict[str, list[Path]] = {"added": [], "present": [], "foreign": []}
    for path in tracked_files(root):
        if args.dry_run or args.check:
            if comment_prefix(path) is None or path.is_symlink() or not path.is_file():
                continue
            text = path.read_text(encoding="utf-8-sig")
            comments = header_comment_lines(text.splitlines(), comment_prefix(path))
            if any(first_notice_line in line for line in comments):
                status = "present"
            elif any(FOREIGN_LICENSE_RE.search(line) for line in comments):
                status = "foreign"
            else:
                status = "added"
        else:
            status = process(path, notice, first_notice_line)
        if status in results:
            results[status].append(path.relative_to(root))

    verb = "Missing notice" if args.check else ("Would add notice" if args.dry_run else "Added notice")
    for path in results["added"]:
        print(f"{verb}: {path}")
    for path in results["foreign"]:
        print(f"Skipped (foreign license header): {path}", file=sys.stderr)
    print(
        f"\n{verb.lower()}: {len(results['added'])}, already present: {len(results['present'])}, "
        f"foreign: {len(results['foreign'])}",
        file=sys.stderr,
    )
    return 1 if args.check and results["added"] else 0


if __name__ == "__main__":
    sys.exit(main())
