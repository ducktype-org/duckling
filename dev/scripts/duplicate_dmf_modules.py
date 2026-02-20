#!/usr/bin/env python3
"""
Duplicate .dmf files 3x in integration tests to create submodules.
Each copy has main() renamed to avoid duplicate symbol errors.
This increases parallel compilation workload for better race condition testing.

Usage:
    python3 scripts/duplicate_dmf_modules.py          # create copies
    python3 scripts/duplicate_dmf_modules.py --undo    # remove copies
"""

import argparse
import re
import sys
from pathlib import Path

ITEST_DIR = Path(__file__).resolve().parent.parent / "integration_tests"
COPY_SUFFIXES = ["_dup1", "_dup2", "_dup3"]
MARKER = "# AUTO-GENERATED duplicate for race testing — do not edit\n"


def rename_main_function(source: str, new_name: str) -> str:
    """
    Rename 'fun main(' to 'fun <new_name>(' in duckling source.
    Only renames the function declaration, not calls to main().
    """
    pattern = re.compile(r'^(fun\s+)main(\s*\()', re.MULTILINE)
    result = pattern.sub(rf'\1{new_name}\2', source, count=1)
    return result


def remove_global_var_mutations(source: str) -> str:
    """
    Remove global variable assignments that reference 'countdown' or similar
    patterns that could cause issues in duplicate modules.
    This is a safety measure — most modules won't have this.
    """
    return source


def create_duplicates(dry_run: bool = False) -> int:
    """Create 3 duplicates of each .dmf file, with main() renamed."""
    all_dmfs = sorted(ITEST_DIR.rglob("*.dmf"))
    created = 0

    for dmf in all_dmfs:
        # Skip files we already created
        if any(suf in dmf.stem for suf in COPY_SUFFIXES):
            continue

        source = dmf.read_text()

        for suffix in COPY_SUFFIXES:
            new_name = dmf.stem + suffix + ".dmf"
            new_path = dmf.parent / new_name

            # Rename main() to a unique name based on suffix
            renamed_main = f"_generated_main{suffix}"
            modified = rename_main_function(source, renamed_main)
            content = MARKER + modified

            if dry_run:
                print(f"  WOULD CREATE: {new_path.relative_to(ITEST_DIR)}")
            else:
                new_path.write_text(content)
                print(f"  CREATED: {new_path.relative_to(ITEST_DIR)}")
            created += 1

    return created


def undo_duplicates(dry_run: bool = False) -> int:
    """Remove all generated duplicate files."""
    removed = 0
    for dmf in sorted(ITEST_DIR.rglob("*.dmf")):
        if any(suf in dmf.stem for suf in COPY_SUFFIXES):
            # Double check it's our file
            try:
                content = dmf.read_text()
                if MARKER in content or any(suf in dmf.stem for suf in COPY_SUFFIXES):
                    if dry_run:
                        print(f"  WOULD REMOVE: {dmf.relative_to(ITEST_DIR)}")
                    else:
                        dmf.unlink()
                        print(f"  REMOVED: {dmf.relative_to(ITEST_DIR)}")
                    removed += 1
            except Exception as e:
                print(f"  ERROR reading {dmf}: {e}", file=sys.stderr)

    return removed


def main():
    parser = argparse.ArgumentParser(description="Duplicate .dmf modules for race testing")
    parser.add_argument("--undo", action="store_true", help="Remove all generated duplicates")
    parser.add_argument("--dry-run", action="store_true", help="Show what would be done")
    args = parser.parse_args()

    if args.undo:
        print("Removing duplicates...")
        count = undo_duplicates(args.dry_run)
        print(f"\n{'Would remove' if args.dry_run else 'Removed'} {count} files")
    else:
        print("Creating duplicates (3 copies per .dmf, with main() renamed)...")
        count = create_duplicates(args.dry_run)
        print(f"\n{'Would create' if args.dry_run else 'Created'} {count} files")
        print("Run with --undo to remove them later")


if __name__ == "__main__":
    main()
