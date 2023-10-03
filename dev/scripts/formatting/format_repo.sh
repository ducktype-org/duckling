#!/bin/bash

# Go to script's location
original_location=$(pwd)
cd "$(dirname "$0")" || exit 1

# Gather files
files=$(find "../" -iname "*build*" -prune -or -iname "*libs*" -prune -or -iname "*.*pp" -print)

# Run the formatting
echo "$files" | xargs ./clang-format --Werror --style=file:"../.clang-format" -i --verbose

# Return to the original location
cd "$original_location" || exit 1
