# CMake

Top level [CMakeLists.txt](../../../../CMakeLists.txt) only provides common options, configurations, and functions. Each module and submodule should have it's own `CMakeLists.txt` file that is included by the one above it.

`CMakeLists.txt` of each module should follow structure similar to:
```cmake
# Create module:
add_library(ModuleName
    path/to/src/some_file.cpp
    ...
)

make_module(ModuleName USES Module1 Module2 ... [INCLUDE path/to/directory1 path/to/directory2])

# Add tests:
duck_add_test(TARGET simple_test path_to_test.cpp
	USES Module1 Module2 ...)  
where $TARGET \in \{common, compiler, vm\}$

# Add examples:
add_example(some_example example_src.cpp USES Module1 Module2 ...)

# Additional executables, options, etc:
# ...

# Add subdirectories:
add_subdirectory(some_sub_module)

```

Some folder-level-modules may hold multiple cmake-level-modules
in which case multiple `add_library + make_module` should be used.
