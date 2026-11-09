#!/bin/bash

# Formats Duckling sources (.dk) in place with duckfmt.
#
# Usage: format_repo_duckling.sh [--all] [--no-untracked] [--check] [duckfmt-binary]
#   default         the files this branch changed, plus the untracked ones
#   --all           every Duckling file in the repository
#   --no-untracked  leave the untracked files alone
#   --check         do not rewrite anything; list the unformatted files and exit non-zero
#   duckfmt-binary  defaults to build/bin/duckfmt (relative to dev/); build it with
#                   `ninja -C build duckfmt`

# Pick the scope of the listing and the mode
scope=(--modified)
untracked=(--include-untracked)
mode=(-i)
while [[ $1 == --* ]]; do
    case $1 in
    --all) scope=() ;;
    --no-untracked) untracked=() ;;
    --check) mode=(-k) ;;
    *)
        echo "Unknown option: $1" >&2
        exit 1
        ;;
    esac
    shift
done

# Go to dev/ directory
cd "$(dirname "$0")"/../../ || exit 1

# Find binary
duckfmt=build/bin/duckfmt
if [[ $1 ]]; then
    duckfmt=$1
fi
if [[ ! -x $duckfmt ]] && ! command -v "$duckfmt" > /dev/null; then
    echo "duckfmt not found at '$duckfmt'. Build it with \`ninja -C build duckfmt\`," >&2
    echo "or pass its path as the last argument." >&2
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
# In --check mode duckfmt itself names every unformatted file on stderr.
status=0
for file in "${files[@]}"; do
    "$duckfmt" "$file" "${mode[@]}" || status=1
done

if [[ ${mode[0]} == -k && $status -ne 0 ]]; then
    echo "Some Duckling files are not formatted. Run scripts/formatting/format_repo_duckling.sh to fix them." >&2
fi
exit $status
