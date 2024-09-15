# Rift source docs

This directory contains documentation for the Rift project. It is divided into two parts: Doxygen and Sphinx. Doxygen is used to document C++ code, while Sphinx is used for more general developer guidelines and implementation ideas. There are useful guides there for developers, like how to write tests and examples, how to commit to our repo, how to write documentation etc.

## Building docs

Assuming current working directory : `dev/`

To create Python virtual environment for Sphinx, run: 
```
./toolbox.py setup-venv
```

To create build directory:
```
./toolbox.py setup-build
```

Now there are two ways to build docs. First one is to use toolbox again:
```
./toolbox.py docs
```
Second one is to use targets created by CMake (with `make` or `ninja`, doesn't matter):
```
cd build
make docs
```

Built docs are placed inside `build/docs/sphinx/` and `build/docs/doxygen/` directories.
To open them, run:
```
make open-sphinx-docs
make open-doxygen-docs
```

Inside you will find more information about building and running the project.
