#!/bin/bash

# Formats C++ sources in place with clang-format.
#
# Usage: format_repo_cpp.sh [--all] [--no-untracked] [clang-format-binary]
#   default         the files this branch changed, plus the untracked ones
#   --all           every C++ file in the repository
#   --no-untracked  leave the untracked files alone

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

# Gather the C++ files. A failed listing must not read as "nothing to format"
listing=$(python3 toolbox.py list-files "${scope[@]}" "${untracked[@]}" --extensions .cpp --extensions .hpp --extensions .cc --extensions .cxx --extensions .h) || exit 1

# One element per file, so a path with spaces and an empty listing both behave.
# A read loop rather than `readarray`: the latter is a bash 4 builtin, and macOS ships bash 3.2,
# where it silently leaves the array empty and the run looks like "nothing to format".
files=()
while IFS= read -r file; do
    [[ -n $file ]] || continue
    files+=("$file")
done <<< "$listing"

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

# Return to the original location, but report clang-format's status
cd "$original_location" || exit 1
exit $status
