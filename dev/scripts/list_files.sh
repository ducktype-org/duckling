#!/bin/bash

# This file lists repo's C++ source files. It's used in format_repo.sh, but also in toolbox's linters.
# Important to note: It lists files relative to `dev/` directory.

# Go to dev/ directory
original_location=$(pwd)
cd "$(dirname "$0")"/.. || exit 1

# Gather files
files=$(find "." -mindepth 1 -iname ".*" -prune -or \( -type d -and -iregex '\./[^/]*build[^/]*' \) -prune -or -ipath "out/build" -prune -or -iname "debug" -prune -or -iname "release" -prune -or -iname "libs" -or -iname "scripts" -prune -or -iname "docs" -prune -or -iname "*.*pp" -print)

# Output files
echo "$files"

# Return to the original location
cd "$original_location" || exit 1
