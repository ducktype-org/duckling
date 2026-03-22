# Dockling: Duckling in Docker aka. running a `dockling-dev` container

## Ensure Docker is installed

You can either follow [official Docker documentation](https://docs.docker.com/get-started/) (we only need Docker Engine... Docker Desktop is some sort of non-root bloat, but should work as well) or just skip to the unimportant bloated webpage part and run the beautifull https://get.docker.com/. You can open this link in your browser and read at the beggining:

```shell
# Usage
# ==============================================================================
#
# To install the latest stable versions of Docker CLI, Docker Engine, and their
# dependencies:
#
# 1. download the script
#
#   $ curl -fsSL https://get.docker.com -o install-docker.sh
#
# 2. verify the script's content
#
#   $ cat install-docker.sh
#
# 3. run the script with --dry-run to verify the steps it executes
#
#   $ sh install-docker.sh --dry-run
#
# 4. run the script either as root, or using sudo to perform the installation.
#
#   $ sudo sh install-docker.sh
```

## Build a docker image
Run the build script:
```shell
dockling-dev/build.sh
```

## Run built dev container

You have two main options: run container directly using Docker CLI or let your VSCode take care of it.

Other IDE's should work similar - just search for their support of dev containers. CLion docs are [here](https://www.jetbrains.com/help/clion/connect-to-devcontainer.html).

### Option 1: Docker CLI - if you like to be in control

Run the run script:
```shell
dockling-dev/run.sh
```

Above command will start the bash inside the container and remove the container once that bash terminates.

You can work with that if you like terminal or "Attach to Running Container" with vscode [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) extension.

### Option 2: Dev Containers extenstion in VSCode

Install [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) extension and "Reopen in Container".

## First compilation inside the container

Due to possible differences in paths on the host and in the container (which venv especially does not like), it is worth initializing the repo with `toolbox.py` only inside the container.

In `/duckling/dev` directory:
```shell
./toolbox.py init
./toolbox.py setup-build
```

And now compilation should work normally. In your build directory (likely `/duckling/dev/build`) run:

```shell
ninja all
```

### You can also use the container only for singular tasks like compiling:

```shell
dockling-dev/run.sh bash -c "cd dev/build; ninja all"
```

Or even copy and modify run sript to be like follows:

```shell
docker ...args... -w /duckling/dev/build dockling-dev ninja all
```
Where `-w` is the same as `--workdir`.
