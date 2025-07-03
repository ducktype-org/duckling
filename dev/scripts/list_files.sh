#!/bin/bash

# This file lists repo's Python files. It's used in format_repo_cpp.sh and format_repo_py.sh.
# Important to note: It lists files relative to `dev/` directory.

# Go to dev/ directory
original_location=$(pwd)
cd "$(dirname "$0")"/.. || exit 1

# Determine the search type
# Default to C++ source files, but can be overridden by passing an argument
search_type=*.*pp
if [[ $1 ]]; then
    search_type=$1
fi
# Gather files
files=$(find "." -mindepth 1 -iname ".*" -prune -or \( -type d -and -iregex '\./[^/]*build[^/]*' \) -prune -or -ipath "out/build" -prune -or -iname "debug" -prune -or -iname "release" -prune -or -iname "libs" -or -iname "scripts" -prune -or -iname "docs" -prune -or -iname "$search_type" -print)

# Output files
echo "$files"

# Return to the original location
cd "$original_location" || exit 1
