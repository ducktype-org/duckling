#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd -P)"

TTY_FLAGS=()
if [ -t 0 ]; then
	TTY_FLAGS=(-it)
fi

USER_FLAG=()
if ! docker info 2>/dev/null | grep -qi rootless; then
	USER_FLAG=(--user "$(id -u):$(id -g)")
fi

set -x

mkdir -p "$SCRIPT_DIR/home_ubuntu"

exec docker run "${TTY_FLAGS[@]}" --rm \
	--name dockling \
	--hostname dockling \
	--cap-add=SYS_PTRACE \
	"${USER_FLAG[@]}" \
	-v "$PROJECT_DIR:/duckling/" \
	-v "$SCRIPT_DIR/home_ubuntu/:/home/ubuntu/" \
	-v "$HOME/.ssh/:/home/ubuntu/.ssh/" dockling-dev "$@"
