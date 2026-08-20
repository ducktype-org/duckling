#!/bin/bash

# Formats C++ sources in place with clang-format.
#
# Usage: format_repo_cpp.sh [--all] [--no-untracked] [clang-format-binary]
#   default         only the files this branch changed (`list-files --modified`), plus untracked ones
#   --all           every C++ file in the repository — what format_repo_cpp_all.sh calls
#   --no-untracked  leave untracked files alone, for a caller that did not check them either

# Pick the scope of the listing
scope=(--modified)
untracked=(--include-untracked)
while [[ $1 == --* ]]; do
    case $1 in
    --all) scope=() ;;
    --no-untracked) untracked=() ;;
    *)
        echo "Unknown option: $1" >&2
        exit 1
        ;;
    esac
    shift
done

# Go to dev/ directory
original_location=$(pwd)
cd "$(dirname "$0")"/../../ || exit 1

# Gather the C++ files. A failure of the listing must not read as "nothing to format",
# so its exit status is checked before the output is used.
listing=$(python3 toolbox.py list-files "${scope[@]}" "${untracked[@]}" --extensions .cpp --extensions .hpp --extensions .cc --extensions .cxx --extensions .h) || exit 1

# One file per array element, so that neither a path containing spaces nor an empty
# listing turns into the wrong argument list (`printf '%s'` keeps the empty listing at
# zero elements, which a here-string would not)
readarray -t files < <(printf '%s' "$listing")

# Check if there is anything to format
if [[ ${#files[@]} -eq 0 ]]; then
    echo "Nothing to format"
    exit 0
fi

# Find binary
clang_format=clang-format-19
if [[ $1 ]]; then
    clang_format=$1
fi

# Run the formatting
"$clang_format" --Werror --style=file:".clang-format" -i --verbose "${files[@]}"
status=$?

# Return to the original location, then report what the formatting itself did — callers
# (cpp-linter, pr-validate) only see the exit status of this script
cd "$original_location" || exit 1
exit $status
