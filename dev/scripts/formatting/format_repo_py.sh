#!/bin/bash

# Go to dev/ directory
original_location=$(pwd)
cd "$(dirname "$0")"/../../ || exit 1

# Gather the Python files. A failed listing must not read as "nothing to format"
listing=$(python3 toolbox.py list-files --extensions .py) || exit 1

# One element per file, so a path with spaces and an empty listing both behave.
# A read loop rather than `readarray`: the latter is a bash 4 builtin, and macOS ships bash 3.2.
files=()
while IFS= read -r file; do
    [[ -n $file ]] || continue
    files+=("$file")
done <<< "$listing"

if [[ ${#files[@]} -eq 0 ]]; then
    echo "Nothing to format"
    exit 0
fi

# Find binary
black=black
if [[ $1 ]]; then
    black=$1
fi

echo "Formatting Python files with $black"

# Call black with all files as separate arguments
"$black" "${files[@]}"

# Return to the original location
cd "$original_location" || exit 1
