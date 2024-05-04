# Main Rift development repository

## Before start

### Installing dependencies

#### Debian/Ubuntu

```bash
sudo apt update -y && \
sudo apt install python3 doxygen graphviz -y
```

#### Arch linux

```bash
sudo pacman -Sy python python-pip doxygen graphviz --noconfirm
```

### Setting up the repo

To begin, enter the `dev/` directory and then:

#### Initialize the repository with toolbox

```bash
./toolbox.py init
```

to initialize the repository (fetches library dependencies, setups virtual environment, downloads binaries, etc...)

#### Create a build folder

```bash
./toolbox.py setup-build
```

to create a build folder. Press enter on every prompt to leave default options.

#### Building the docs

```bash
./toolbox.py docs
```

to build docs and open them in your favorite browser. Leave default if you chose defaults in previous step.

___

You can read more about toolbox'es useful features at:

```bash
./toolbox.py --help
```

or for more specific information about a command:

```bash
./toolbox.py setup-build --help
```

## File structure

* [dev](dev/) - main code development

## Making changes

* See docs
* [dev/guidelines](guidelines/) - guidelines dedicated to writing code
