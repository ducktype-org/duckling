#!/bin/bash

# Formats every C++ file in the repository, instead of only the ones this branch changed.
# The two entry points differ solely in that scope, so this is a thin wrapper: the actual
# formatting lives in format_repo_cpp.sh, which takes `--all` for exactly this.
#
# Usage: format_repo_cpp_all.sh [clang-format-binary]

exec "$(dirname "$0")"/format_repo_cpp.sh --all "$@"
