#!/bin/bash

# Go to dev/ directory
original_location=$(pwd)
cd "$(dirname "$0")"/../../ || exit 1

# Gather files
files=$(./scripts/list_files.sh)

# Run the formatting
echo "$files" | xargs ./scripts/downloads/clang-format --Werror --style=file:".clang-format" -i --verbose

# Return to the original location
cd "$original_location" || exit 1
