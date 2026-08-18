#!/bin/bash

# Go to dev/ directory
original_location=$(pwd)
cd "$(dirname "$0")"/../../ || exit 1

# Gather C++ files into an array, one file per element, so that neither a path
# containing spaces nor an empty listing turns into the wrong argument list
readarray -t files < <(python3 toolbox.py list-files --include-untracked --extensions .cpp --extensions .hpp --extensions .cc --extensions .cxx --extensions .h)

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

# Return to the original location
cd "$original_location" || exit 1
