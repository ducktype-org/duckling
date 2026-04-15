include(FetchContent)

# Downloads and sets up tree sitter parser for usage with c++

set(TREE_SITTER_COMMIT "534c4a074cd461ab30d1c8a54bf733d3050221a0")  # 0.26.6, released 2026-02-25
set(TREE_SITTER_CPP_COMMIT "f41e1a044c8a84ea9fa8577fdd2eab92ec96de02")  # 0.23.4, released 2024-11-11

set(TS_URL "https://github.com/tree-sitter/tree-sitter/archive/${TREE_SITTER_COMMIT}.zip")
set(TS_CPP_URL "https://github.com/tree-sitter/tree-sitter-cpp.git")

FetchContent_Declare(
	tree-sitter
	URL ${TS_URL}
    SYSTEM
)

FetchContent_Declare(
	tree-sitter-cpp
    GIT_REPOSITORY ${TS_CPP_URL}
    GIT_TAG ${TS_CPP_COMMIT}
    SYSTEM
)

FetchContent_MakeAvailable(tree-sitter tree-sitter-cpp)
target_include_directories(tree-sitter-cpp INTERFACE "${tree-sitter-cpp_SOURCE_DIR}/bindings/c")
