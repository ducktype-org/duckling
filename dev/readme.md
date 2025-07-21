\page dev-readme ReadMe

# Main development folder

## File structure

* [dependencies](dependencies/) - CMake files for dealing with dependencies,
* [docs](docs/) - developer documentation,
* [scripts](scripts/) - collection of various scripts.
* [src/base](src/base/) - our custom standard library, should be preferred over `std::`
* [src/common](src/common/) - modules shared across the entire codebase,
* [src/compiler](src/compiler/) - implementation of the main Duckling language compiler,
* [src/DucklingLS](src/DucklingLS/) - implementation of the Duckling language server and VS Code client,
* [src/playground](src/playground/) - a space for trying out new ideas and experiments,
* [src/VM](src/vm/) - implementation of the Duckling Virtual Machine,


## Module structure

```
📦module_name
 ┣ 📂src
 ┃ ┗ 📂module_name
 ┃ ┃ ┣ 📜source.hpp
 ┃ ┃ ┣ 📜source.cpp
 ┃ ┃ ┗ 📜other_source_files
 ┣ 📂tests
 ┃ ┣ 📜CMakeLists.txt
 ┃ ┣ 📜some_file.txt
 ┃ ┣ 📜some_file.cpp
 ┃ ┗ 📜test.cpp
 ┣ 📜CMakeLists.txt
 ┗ 📜readme.md
```

Tests are treated as a submodule. There might also be some other additional submodules.
Paths like `module_name/src/module_name` are not pretty, but are necessary to provide correct include paths using CMake.
