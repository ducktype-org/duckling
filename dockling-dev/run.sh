#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd -P)"

USER_GROUP="$(id -u):$(id -g)"

TTY_FLAGS=()

if [ -t 0 ]; then
  TTY_FLAGS=(-it)
fi

set -x

mkdir -p "$SCRIPT_DIR/home_ubuntu"

exec docker run "${TTY_FLAGS[@]}" --rm \
	--name dockling \
	--hostname dockling \
	--user "$USER_GROUP" \
	--cap-add=SYS_PTRACE \
	-v "$PROJECT_DIR:/duckling/" \
	-v "$SCRIPT_DIR/home_ubuntu/:/home/ubuntu/" \
	-v "$HOME/.ssh/:/home/ubuntu/.ssh/" dockling-dev "$@"
