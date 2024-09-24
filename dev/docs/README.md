# Rift source docs

This directory contains documentation for the Rift project. It is divided into two parts: Doxygen and Sphinx. Doxygen is used to document C++ code, while Sphinx is used for more general developer guidelines and implementation ideas. 
It includes helpful guides for developers, such as instructions on writing tests and examples, contributing to the repository, and creating documentation.

## Building docs

**Detailed instruction on how to build the docs can be found in main [README.md](https://github.com/ducktype-org/rift-dev?tab=readme-ov-file#initialize-the-repository-with-toolbox) file.**

Make sure you have Python virtual environment set up (typically with `toolbox.py`), 
as well as build directory created with `docs` option enabled.

### Old method

You can also use `CMake` target to build the docs (once you have the build directory correctly set up).

To build the docs, run:
```
make docs
```

Generated docs are placed inside `build/docs/sphinx/` and `build/docs/doxygen/` directories.
To open them, run:
```
make open-sphinx-docs
make open-doxygen-docs
```
