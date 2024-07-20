\page dev-readme Folder Structure

# Main development folder

## File structure

* [RiftCompiler](RiftCompiler/) - implementation of main Rift language compiler
* [RiftVM](RiftVM/) - implementation of Rift Virtual Machine
* [common](common/) - modules shared across entire codebase
* [guidelines](guidelines/) - guidelines related to development
* [miscellaneous](miscellaneous/) - for files without any specific location
* [libs](libs/) - for external libraries

## Code structure

### General structure

[RiftCompiler](RiftCompiler/) and [RiftVM](RiftVM/) are modules by themselves, while
`common` folder holds subfolders each representing a single module.

### Module structure

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

Tests are treated as a submodule. There might also by some other additional submodules.
Paths like `module_name/src/module_name` are not pretty, but are necessary to provide correct include paths using CMake.

### CMake

---> [CMakeGuidelines.md](guidelines/CMakeGuildelines.md)
