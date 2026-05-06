include(FetchContent)

# Downloads and sets up tree sitter parser for usage with c++

set(TREE_SITTER_COMMIT "cd5b087cd9f45ca6d93ab1954f6b7c8534f324d2")  # 0.26.8, released 2026-03-31
set(TREE_SITTER_CPP_COMMIT "f41e1a044c8a84ea9fa8577fdd2eab92ec96de02")  # 0.23.4, released 2024-11-11

set(TS_URL "https://github.com/tree-sitter/tree-sitter/archive/${TREE_SITTER_COMMIT}.zip")
set(TS_CPP_URL "https://github.com/tree-sitter/tree-sitter-cpp/archive/${TREE_SITTER_CPP_COMMIT}.zip")

FetchContent_Declare(
	tree-sitter
	URL ${TS_URL}
    SYSTEM
)

FetchContent_Declare(
	tree-sitter-cpp
    URL ${TS_CPP_URL}
    # See the file to know why this is a copy and not an actual patch.
    PATCH_COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_LIST_DIR}/patches/tree-sitter-cpp-CMakeLists.txt" CMakeLists.txt
    SYSTEM
)

FetchContent_MakeAvailable(tree-sitter tree-sitter-cpp)
target_include_directories(tree-sitter-cpp INTERFACE "${tree-sitter-cpp_SOURCE_DIR}/bindings/c")
