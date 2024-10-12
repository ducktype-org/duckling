# Main development folder

## File structure

* [base](base/) - our custom standard library, should be preferred over `std::`
* [common](common/) - modules shared across the entire codebase,
* [dependencies](dependencies/) - CMake files for dealing with dependencies,
* [docs](docs/) - developer documentation,
* [DucklingLS](DucklingLS/) - implementation of the Duckling language server and VS Code client,
* [miscellaneous](miscellaneous/) - for files without any specific location,
* [playground](playground/) - a space for trying out new ideas and experiments,
* [compiler](compiler/) - implementation of the main Duckling language compiler,
* [VM](VM/) - implementation of the Duckling Virtual Machine,
* [scripts](scripts/) - collection of various scripts.


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
