#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd -P)"
HOME_DIR="$SCRIPT_DIR/home"

TTY_FLAGS=()
if [ -t 0 ]; then
	TTY_FLAGS=(-it)
fi

set -x

mkdir -p "$HOME_DIR"

exec docker run "${TTY_FLAGS[@]}" --rm \
	--name dockling \
	--hostname dockling \
	--cap-add=SYS_PTRACE \
	-v "$PROJECT_DIR:/duckling/" \
	-v "$HOME_DIR:/home/ubuntu/" \
	-v "$HOME/.ssh/:/home/ubuntu/.ssh/" dockling-dev "$@"
