# ModuleTree

The **ModuleTree** component provides a hierarchical, in-memory representation of a Duckling project's module structure. It is designed to be **language server-ready**: all mutations are performed in a controlled way via the `ModuleTreeModifier` interface, which is intended for use by the Language Server (LS) or similar tooling.

## Overview

- **ModuleTree**: Represents a single module, including its source files, submodules, and other files.
- **ModuleTreeBuilder**: Constructs a ModuleTree from a filesystem directory or file, applying customizable file/directory filters.
- **ModuleTreeModifier**: Provides controlled mutation operations for modules (e.g., adding/removing files or submodules). This interface is specifically designed for Language Server scenarios and should be used for all dynamic changes.
- **SourceFile**: Represents a source file within a module, with cached content and lazy parsing.

## Key Features

- **Automatic Parsing**: Recursively builds the module tree from a directory or single-file module.
- **Filtering**: Supports regex-based filtering of files and directories (e.g., skip hidden or special files).
- **Source File Management**: Tracks source files, main source file, and other files by extension.
- **Submodule Support**: Each module can have submodules, forming a tree structure.
- **Pretty Printing**: Human-readable output of the module tree for debugging or inspection.
- **Language Server Ready**: All mutations are explicit and safe for incremental and live-editing scenarios.

## Example Usage

```cpp
#include <frontend/module_tree/module_tree.hpp>

fs::File root_dir("path/to/project");
auto module_tree = compiler::frontend::ModuleTreeBuilder::create(root_dir, "package_id");

// Access submodules
for (const auto& [name, submodule] : module_tree->getSubmodules()) {
    std::cout << "Submodule: " << name.strView() << std::endl;
}

// List source files
for (const auto& src : module_tree->getSourceFiles()) {
    std::cout << "Source: " << src->getFile().name() << std::endl;
}

// Pretty print the whole tree
std::cout << module_tree->prettyPrint();
```

## Notes

- The module tree is immutable after construction; use `ModuleTreeModifier` for controlled changes. This is especially important for Language Server implementations, where live updates and incremental changes are required.
- Source files are cached and can be lazily parsed into syntax trees.
- Filtering regexes can be customized to control which files and directories are included.
- The current design is intended to be compatible with Language Server needs, but **may require changes in the future** as the final Language Server architecture is established.

See the test suite for more usage examples.
