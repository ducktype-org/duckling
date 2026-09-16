# Filesystem Module

The **Filesystem Module** represents the final abstraction layer in Duckling between the real filesystem and the internal compiler operations. It provides a unified interface for handling different types of files and directories through the `File` class and `FileManager` utilities.

## Overview

The `File` class abstracts and unifies operations across three different file types:
- **Physical files**: Files that exist on the actual filesystem
- **Virtual files**: Files managed by the internal Virtual Filesystem (VFS)  
- **Temporary files**: System-managed temporary files

The key design principle is that `File` provides the same operations regardless of which type of file it represents, creating a seamless experience for file manipulation.

## Core Components

### File Class
The `File` class can represent both **files** and **directories**. It provides:
- Unified access to content via `base::SharedView` objects
- Content reading/writing operations
- Directory listing capabilities

### FileManager Class
The `FileManager` provides static factory methods and utilities for:
- Creating files and directories of different types
- Path conversions between virtual and physical paths
- File existence checks and deletion operations
- Managing file operations across different filesystem types

### FilePath Class
The `FilePath` class provides a safe wrapper around `std::filesystem::path` with enhanced functionality for different filesystem types:
- **Type-aware paths**: Automatically detects and tracks whether a path is Physical, Virtual, or Temporary
- **Safe path operations**: Provides path manipulation without dangerous canonicalization
- **Cross-filesystem conversion**: Convert between virtual and physical path representations
- **Unified interface**: Same API regardless of the underlying path type

```cpp
#include <filesystem/file_path.hpp>

// Create different types of paths
fs::FilePath physical_path("/home/user/file.txt");
fs::FilePath virtual_path("vfs:/project/source.cpp");

// Convert between path types
auto vfs_path = physical_path.toVirtualPath();
auto real_path = virtual_path.toPhysicalPath();
auto joined = physical_path.join("subfolder/file.txt");

// Type checking
if (path.isVirtual()) {
    // Handle virtual path
} else if (path.isPhysical()) {
    // Handle physical path
}
```

## Virtual Filesystem Usage

The Virtual Filesystem (VFS) is particularly useful for scenarios where you need a legitimate filesystem that can handle queries and modifications without touching the actual disk:

```cpp
#include <filesystem/file.hpp>

// Create a virtual root directory
auto virtual_root = fs::FileManager::createRandomVirtualDirectory();

// Add a file at a specific path within the virtual filesystem
auto config_file = virtual_root.createSubFile(
    "debug=true\nversion=1.0", 
    "config.txt"
);

// Read the file content at any time
auto content = config_file.getContent();
std::cout << "Config: " << content.view().stringView() << std::endl;

// Modify the file content at any time
config_file.writeToFile("debug=false\nversion=1.1");

// Filesystem structure
auto files = virtual_root.listFilePaths();
for (const auto& file : files) {
    std::cout << "Found file: " << file.name() << std::endl;
}
```

### Key VFS Capabilities:
1. **Create virtual root**: Establish a virtual filesystem root
2. **Add files at specific paths**: Place files exactly where needed in the virtual structure
4. **Modify file content**: Update file contents dynamically
5. **Legitimate filesystem operations**: The VFS supports real filesystem queries and operations
6. **Attach metadata**: Store arbitrary per-file data next to the content

### File Metadata

A virtual file can also carry arbitrary metadata, held as a `std::any`. This is meant for code that
needs its own bookkeeping per file — the language server, for example, tracks the editor's document
version and language id. Without it you would need a second map keyed by path and kept in sync by
hand.

```cpp
struct DocumentInfo {
    std::string language_id;
    i32         version;
};

auto file = fs::FileManager::createRandomVirtualFile("let x = 1", ".dk");

file.writeMetadata(DocumentInfo{ .language_id = "duckling", .version = 1 });

if (file.hasMetadata()) {
    auto info = file.getMetadataAs<DocumentInfo>();  // throws if it holds another type
    std::cout << "version: " << info.version << std::endl;
}

file.clearMetadata();
```

Three things to know about it:
- **Only virtual files carry metadata.** Calling these methods on a physical or temporary file
  panics, because there is nowhere on disk to put it.
- **Content and metadata are independent.** `writeToFile` leaves the metadata alone, and
  `writeMetadata` leaves the content alone.
- **Metadata dies with the file.** Deleting the file drops it, so a file recreated under the same
  path starts out with no metadata.

## Path Conversion Utilities

The `FileManager` provides crucial path conversion functions for bridging physical and virtual filesystems:

### toVirtualPath()
Converts a physical filesystem path to a virtual path by prefixing with the VFS root:

```cpp
// Convert physical path to virtual path
std::filesystem::path physical_path = "/home/user/source.cpp";
auto virtual_path = physical_path.toVirtualPath();
// Result: "vfs:/home/user/source.cpp"
```

### fromVirtualPath()
Converts a virtual path back to a physical path by removing the VFS root prefix:

```cpp
// Convert virtual path back to physical path
std::filesystem::path virtual_path = "vfs:/home/user/source.cpp";
auto physical_path = virtual_path.toPhysicalPath();
// Result: "/home/user/source.cpp"
```

### Usage Example
These conversions are particularly useful when you need to map between real files and their virtual representations:

```cpp
// Start with a physical file
auto physical_file = fs::FileManager::createPhysicalFile("./test.txt", "original content");

// Copy physical file content to virtual filesystem
auto physical_content = physical_file.getContent();
auto content_string = physical_content.view().stringView();
auto virtual_file = fs::FileManager::createVirtualFile(physical_file.getPath(.toVirtualPath()), content_string);

// Now both files exist independently - physical and virtual
```

## File vs FileManager Separation

The design separates concerns between:

- **File**: Represents a specific file or directory and provides instance methods for content access, metadata, and file-specific operations
- **FileManager**: Provides static factory methods and utilities for creating, managing, and converting between different file types

This separation ensures that:
- File objects focus on representing and accessing individual files
- FileManager handles cross-cutting concerns like creation, conversion, and filesystem-wide operations
- The API remains clean and intuitive for different use cases

## Example: Complete Virtual Filesystem Workflow

```cpp
// Create virtual filesystem
auto vfs_root = fs::FileManager::createRandomVirtualDirectory();

// Create nested directory structure
auto src_dir = vfs_root.createSubDirectory("src");
auto main_file = src_dir.createSubFile(
    "#include <iostream>\nint main() { return 0; }", 
    "main.cpp"
);

// Query and modify
auto files = src_dir.listFilePaths();
main_file.writeToFile("#include <iostream>\n\nint main() {\n    std::cout << \"Hello!\";\n    return 0;\n}");

// Access content
auto updated_content = main_file.getContent();
std::cout << "Updated source:\n" << updated_content.view().stringView() << std::endl;
```

This module provides the foundation for all file operations within Duckling, ensuring consistent behavior across different file types while maintaining the flexibility needed for compiler operations.
