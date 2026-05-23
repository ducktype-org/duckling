# Dockling: Running Duckling in Docker (`dockling-dev`)


## Prerequisites

Ensure that Docker is installed on your system. Follow [the official documentation](https://docs.docker.com/get-started/get-docker/) or use the official installation script available at:

- [https://get.docker.com/](https://get.docker.com/) for standard (rootful) instalation

or

- [https://get.docker.com/rootless](https://get.docker.com/rootless) for [rootless mode](https://docs.docker.com/engine/security/rootless/) instalation

Example installation workflow:

```shell
# Download the installation script
curl -fsSL https://get.docker.com -o install-docker.sh

# Review the script (recommended)
cat install-docker.sh

# Optional: perform a dry run
sh install-docker.sh --dry-run

# Install Docker (requires root or sudo privileges)
sudo sh install-docker.sh
```


## Running container

Two supported workflows are available:

### 1. Docker CLI

Build the `dockling-dev` image:

```shell
dockling-dev/build.sh
```

Run the container directly:

```shell
dockling-dev/run.sh
```

This starts an interactive shell inside the container. The container is automatically removed when the session exits.

You may also attach to the running container using tools such as Visual Studio Code (Dev Containers extension).

---

### 2. VS Code Dev Containers

Install the [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) extension for Visual Studio Code and use **“Reopen in Container”** command (from automiatic pop-up or command palette under F1 key), then choose devcontainer corresponding to your docker instalation.

Other IDEs offer similar functionality. For example, **CLion** provides [support for development containers](https://www.jetbrains.com/help/clion/connect-to-devcontainer.html).


## Initial Project Setup (Inside Container)

To avoid issues caused by differences between host and container environments (e.g., virtual environment paths), initialize the repository inside the container only.

You can use provided `toolbox` command (with bash autocompletion!) that runs `toolbox.py` script with workdir automatically set to `/duckling/dev`.

```shell
toolbox init
toolbox setup-build
```


## Building Project

As usual run the build from the build directory (typically `/duckling/dev/build`):

```shell
ninja all
```

## Good To Know

### Toolbox command

`toolbox` always runs `toolbox.py` in `/duckling/dev`.

This mean can use `toolbox` comand like `toolbox test -b build` no matter what directory are you in as long as `/duckling/dev/build` exists

### Mounts

By default, *the dev container configuration* and *`run.sh` script* mount three directories:

- duckling repo → `/duckling`

- `dockling-dev/home` → `/home/ubuntu` - to make your home directory in the container persistent, so your configuration is preserved between runs. You can access it and/or delete it from outside of the container.

- `$HOME/.ssh` → `/home/ubuntu/.ssh` - allows Git inside the container to authenticate with GitHub. **SECURITY DISCLAIMER: this directory usually stores confidential data. Mounting it means it's accesible by programs running inside the container the same way it is accessible from any other program running on your system.**

### Non-Interactive Usage

For one-off commands:

```shell
dockling-dev/run.sh bash -c "cd dev/build; ninja all"
```

Alternatively, you may adapt the run script to execute commands directly:

```shell
docker ...args... -w /duckling/dev/build dockling-dev ninja all
```

The `-w` (`--workdir`) flag sets the working directory inside the container.

### Run Script Limitations

It sets the name of the container, allowing only one running instance, but making it easier to recognise the container when connecting from VSCode. You can comment this line out if you want.
