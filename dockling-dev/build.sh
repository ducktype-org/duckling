SCRIPT_DIR=$(dirname -- "${BASH_SOURCE[0]}")

docker build "$SCRIPT_DIR" -t dockling-dev
