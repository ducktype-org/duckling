#!/bin/bash

# Go to dev/ directory
original_location=$(pwd)
cd "$(dirname "$0")"/../../ || exit 1

# Gather files into array
readarray -t files < <(./scripts/list_py_files.sh)

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
