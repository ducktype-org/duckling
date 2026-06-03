#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(dirname -- "${BASH_SOURCE[0]}")

DEFAULT_USER=()
if docker info 2>/dev/null | grep -qi rootless; then
	echo "Detected rootless docker, changind default container user to root."
	DEFAULT_USER=(--build-arg DEFAULT_USER=root)
fi

set -x

docker build \
	--build-arg UID="$(id -u)" \
	--build-arg GID="$(id -g)" \
	"${DEFAULT_USER[@]}" \
	-t dockling-dev \
	"$SCRIPT_DIR/image"
