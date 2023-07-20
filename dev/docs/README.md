**Docs might temporary be broken due to migration to submodule**

# Rift the docs docs

- [Rift the docs docs](#rift-the-docs-docs)
  - [Building](#building)
  - [Excluded directories](#excluded-directories)
  - [Writing docs](#writing-docs)

## Building

Assuming current working directory : `dev/`

In order to build one must first install all the dependencies listed in `requirements.txt`:

```sh
python3 -m venv .venv
source .venv/bin/activate
pip3 install -r docs/doc-config/requirements.txt
```

**Python dependencies must be installed before running CMake!**

Also following dependencies are needed:

- Doxygen

Installing deps:

- Debian (and derivatives):

  ```sh
  sudo apt update
  sudo apt install doxygen
  ```

- Arch Linux (btw, I use arch):

  ```sh
  sudo pacman -Syu doxygen
  ```

Then, from the `dev/` directory run

```sh
cmake -B build
cd build
make docs
```

Built docs are placed inside `build/docs/sphinx/` directory.

## Excluded directories

Doxygen does not perform parsing of `*/libs/*` directories.

## Writing docs

Helpful reference: [doyxgenclass](https://breathe.readthedocs.io/en/latest/class.html)

@TODO: This section requires a debate
