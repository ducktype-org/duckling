# Duckling source docs

This directory contains documentation for the Duckling project in Doxygen whitch is used to document C++ code.
It includes helpful guides for developers, such as instructions on writing tests and examples, contributing to the repository, and creating documentation.

## Building docs

**Detailed instruction on how to build the docs can be found in main [README.md](https://github.com/ducktype-org/duckling?tab=readme-ov-file#initialize-the-repository-with-toolbox) file.**

Make sure you have Python virtual environment set up (typically with `toolbox.py`), 
as well as build directory created with `docs` option enabled.

### Old method

You can also use `CMake` target to build the docs (once you have the build directory correctly set up).

To build the docs, run:
```
make docs
```

Generated docs are placed inside `build/docs/doxygen/` directories.
To open them, run:
```
make open-doxygen-docs
```
