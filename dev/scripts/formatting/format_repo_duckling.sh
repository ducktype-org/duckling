#!/bin/bash

# Formats Duckling sources (.dk) in place with duckfmt.
#
# Usage: format_repo_duckling.sh [--all] [--no-untracked] [--check] [duckfmt-binary]
#   The options and the binary can be given in any order.
#   default         the files this branch changed, plus the untracked ones
#   --all           every Duckling file in the repository
#   --no-untracked  leave the untracked files alone
#   --check         do not rewrite anything, only list the unformatted files and exit non-zero
#   duckfmt-binary  a path (relative to the current directory) or a name on the PATH.
#                   Defaults to dev/build/bin/duckfmt, build it with `ninja -C build duckfmt`

# Parse the arguments in any order. Every argument starting with `--` is an option, others are interpreted as a file to format.
scope=(--modified)
untracked=(--include-untracked)
mode=(-i)
all_flag=""
duckfmt_arg=""
for arg in "$@"; do
    case $arg in
    --all)
        scope=()
        all_flag=" --all"
        ;;
    --no-untracked) untracked=() ;;
    --check) mode=(-k) ;;
    --*)
        echo "Unknown option: $arg" >&2
        exit 1
        ;;
    *)
        if [[ $duckfmt_arg ]]; then
            echo "More than one duckfmt binary given: '$duckfmt_arg' and '$arg'" >&2
            exit 1
        fi
        duckfmt_arg=$arg
        ;;
    esac
done

# Find binary. A path given by the user is resolved before the `cd` below, so it is read
# from where the script was run.
duckfmt=build/bin/duckfmt
if [[ $duckfmt_arg == */* ]]; then
    duckfmt_dir=$(cd "$(dirname "$duckfmt_arg")" 2> /dev/null && pwd) || duckfmt_dir=$(dirname "$duckfmt_arg")
    duckfmt=$duckfmt_dir/$(basename "$duckfmt_arg")
elif [[ $duckfmt_arg ]]; then
    duckfmt=$duckfmt_arg
fi

# Go to dev/ directory
cd "$(dirname "$0")"/../../ || exit 1

if [[ ! -x $duckfmt ]] && ! command -v "$duckfmt" > /dev/null; then
    echo "duckfmt not found at '$duckfmt'. Build it with \`ninja -C build duckfmt\`," >&2
    echo "or pass its path as an argument." >&2
    exit 1
fi

# Gather the Duckling files. A failed listing must not read as "nothing to format"
listing=$(python3 toolbox.py list-files "${scope[@]}" "${untracked[@]}" --filter-deleted --extensions .dk) || exit 1

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

# duckfmt takes a single file, so run it once per file and remember any failure.
# It prints the reason on stderr itself. It exits with 1 when --check finds an unformatted
# file and with 2 when it cannot format the file at all (a lexer error, for example).
unformatted=0
failed=0
for file in "${files[@]}"; do
    "$duckfmt" "$file" "${mode[@]}"
    case $? in
    0) ;;
    1) unformatted=1 ;;
    *) failed=1 ;;
    esac
done

if [[ $failed -ne 0 ]]; then
    echo "duckfmt failed on some Duckling files, see the errors above." >&2
fi
if [[ $unformatted -ne 0 && ${mode[0]} == -k ]]; then
    echo "Some Duckling files are not formatted. Run scripts/formatting/format_repo_duckling.sh$all_flag to format them." >&2
fi
if [[ $failed -ne 0 || $unformatted -ne 0 ]]; then
    exit 1
fi
exit 0
