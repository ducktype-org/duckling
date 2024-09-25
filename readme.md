# Main Rift development repository

## Before start

### Installing dependencies

#### Debian/Ubuntu

```bash
sudo apt update -y && \
sudo apt install python3 python3-click doxygen graphviz-dev cmake ninja-build g++ lcov llvm-dev clang-tidy libzstd-dev zlib1g-dev -y
```

#### Arch linux

```bash
sudo pacman -Sy python python-pip python-click doxygen graphviz lcov --noconfirm
```

### Setting up the repo

To begin, enter the `dev/` directory and then:

#### Initialize the repository with toolbox

```bash
./toolbox.py init
```

This:
* fetches library dependencies
* updates git submodules
* setups virtual environment
* downloads binaries, etc...

#### [Optional, but recommended] Installing custom LLVM library

```bash
./toolbox.py download-llvm
```

In case of trouble during build or `setup-build` step, it is advised
to install the latest supported version of LLVM for a given machine.
After using this command, 
LLVM is **NOT installed system-wide**, but only for this project.
Using version `18.1.8` should work for most platforms.

#### Create a build folder

```bash
./toolbox.py setup-build
```

Press enter on every prompt to leave default options.

#### Building the docs

If you want to build the docs make sure that in the previous step 
the build directory was created with the `docs` option enabled.

```bash
./toolbox.py docs
```

This builds the docs and opens them in your favorite browser. 
Leave defaults if you chose defaults in previous step.

#### Running tests

```shell
./toolbox.py test     # regular tests
./toolbox.py test -m  # run tests under valgrind
```

#### Testing coverage

Before running this command make sure you build folder has enabled coverage.

```shell
# This is only needed if build folder was not prepared for coverage.
./toolbox.py setup-build --coverage

./toolbox.py coverage
```


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
