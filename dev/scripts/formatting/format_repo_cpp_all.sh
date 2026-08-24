#!/bin/bash

# Formats every C++ file in the repository, not only the ones this branch changed.
# Usage: format_repo_cpp_all.sh [--no-untracked] [clang-format-binary]

exec "$(dirname "$0")"/format_repo_cpp.sh --all "$@"
