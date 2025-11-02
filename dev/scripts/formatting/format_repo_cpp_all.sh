#!/bin/bash

# Go to dev/ directory
original_location=$(pwd)
cd "$(dirname "$0")"/../../ || exit 1

# Gather C++ files
files=$(python3 toolbox.py list-files --extensions .cpp --extensions .hpp --extensions .cc --extensions .cxx --extensions .h)
# Find binary
clang_format=clang-format-19
if [[ $1 ]]; then
    clang_format=$1
fi

# Run the formatting
echo "$files" | xargs $clang_format --Werror --style=file:".clang-format" -i --verbose

# Return to the original location
cd "$original_location" || exit 1
