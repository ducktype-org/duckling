#!/usr/bin/env bash

SCRIPT_DIR=$(dirname -- "${BASH_SOURCE[0]}")

docker build \
	--build-arg UID="$(id -u)" \
	--build-arg GID="$(id -g)" \
	-t dockling-dev \
	"$SCRIPT_DIR/image"
