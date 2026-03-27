SCRIPT_DIR=$(dirname -- "${BASH_SOURCE[0]}")

mkdir -p "$SCRIPT_DIR"/home_ubuntu

docker run -it --rm \
	--name "dockling" \
	-h "dockling" \
	--user "$(id -u):$(id -g)" \
	--cap-add=SYS_PTRACE \
	-v "$SCRIPT_DIR"/..:/duckling \
	-v "$SCRIPT_DIR"/home_ubuntu/:/home/ubuntu/ \
	-v "$HOME"/.ssh:/home/ubuntu/.ssh dockling-dev "$@"
