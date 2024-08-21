**Docs might temporarily be broken due to migration to submodule**

# Rift source docs and dev guide

- [Rift source docs and dev guide](#rift-source-docs-and-dev-guide)
  - [Building](#building)
  - [Excluded directories](#excluded-directories)
  - [Writing docs](#writing-docs)
  - [Writing examples](#writing-examples)

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
- graphviz

Installing deps:

- Debian (and derivatives):

  ```sh
  sudo apt update
  sudo apt install doxygen graphviz
  ```

- Arch Linux (btw, I use arch):

  ```sh
  sudo pacman -Syu doxygen graphviz
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

There are two documentation sources: Doxygen and Sphinxs.
Doxygen is used to directly document C++ code and write descriptions of our modules, while Sphinx is used for more general 
developer guidelines and implementation ideas.

Code documentation is written in Doxygen next to the source code. 
To write more general descriptions of modules or groups of files create Markdown file (e.g. `readme.md`) and add the Doxygen "@page" command at the top of it.
```
@page <page_reference_name> <Page title>
```
To add subpage write in the parent page:
```
@subpage <child_page_reference_name>
```
This will create a tree of pages.

Doxygen autolink feature makes a link from every function name, class, file name that you write anywhere. You only have to write the name.
If you want to link to a file, for example `queries.hpp`, and there is more than one file of that name in the project, you can write more of the path to that file, for example `helios/queries.hpp`. 

Basides pages, documentation of each file is written as a Doxygen commment at the __top of each file__.

Currently, there Doxygen generated different trees and different websites for "pages" and for "files".
There is no automatic way to link on page all visible files or all files inside some directory.
To fix that in the near future we will move "pages" to "dir" - directory documentation marked by "@dir" Doxygen command.


@TODO Create tool to automatically link all files containing docs visible from page to that page.

## Writing examples

When writing documentation you can put examples hard-coded inside code block or put it in outside file.
The latter has some advantages, because the Doxygen entities (functions, classes, macros) used inside examples 
will have a link to that example file on them.
To mark file as example you can write:
```
@example example_file_name.cpp

...example description...
```
Since doxygen distinguishes between examples based on the file name, it is a good idea for it to be unique.
You can also put file content inside Doxygen documentation using `@include` command.

@TODO: This section requires a debate
