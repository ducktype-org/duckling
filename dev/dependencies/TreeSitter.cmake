# Downloads and sets up tree sitter parser for usage with c++


set(TREE_SITTER_COMMIT "12fb31826b8469cc7b9788e72bceee5af1cf0977")
set(TREE_SITTER_CPP_COMMIT "30f973c2244f0bff444186185f475c3bd76bc3a5")

include(FetchContent)

set(DEPS_DIR ${PROJECT_BINARY_DIR}/_deps)

set(TS_DIR ${DEPS_DIR}/tree-sitter-src)
set(TS_CPP_DIR ${DEPS_DIR}/tree-sitter-cpp-src)

set(TS_LIB ${TS_DIR}/libtree-sitter.a)
set(TS_CPP_LIB ${TS_CPP_DIR}/libtree-sitter-cpp.a)

set(TS_INCLUDE ${TS_DIR}/lib/include)
set(TS_CPP_INCLUDE ${TS_CPP_DIR}/bindings/c)

FetchContent_Declare(
    fetch_tree_sitter
    URL "https://github.com/tree-sitter/tree-sitter/archive/${TREE_SITTER_COMMIT}.zip"
    SOURCE_DIR ${TS_DIR}
)

FetchContent_Declare(
    fetch_tree_sitter_cpp
    URL "https://github.com/tree-sitter/tree-sitter-cpp/archive/${TREE_SITTER_CPP_COMMIT}.zip"
    SOURCE_DIR ${TS_CPP_DIR}
)


FetchContent_MakeAvailable(fetch_tree_sitter fetch_tree_sitter_cpp)

# In the following targets the `make` command is used,
# not the ${CMAKE_MAKE_PROGRAM}, as tree sitter only provides
# `Makefile`s and does not support other build systems.
add_custom_target(
	build_tree_sitter
	BYPRODUCTS ${TS_LIB}
	COMMAND make --quiet
	WORKING_DIRECTORY ${TS_DIR}
	VERBATIM
)

add_custom_target(
	build_tree_sitter_cpp
	BYPRODUCTS ${TS_CPP_LIB}
	COMMAND make --quiet
	WORKING_DIRECTORY ${TS_CPP_DIR}
	VERBATIM
)

add_library(tree_sitter STATIC IMPORTED GLOBAL)
add_dependencies(tree_sitter build_tree_sitter)
set_target_properties(tree_sitter PROPERTIES IMPORTED_LOCATION`
	${TS_LIB})
target_include_directories(tree_sitter INTERFACE ${TS_INCLUDE})

add_library(tree_sitter_cpp STATIC IMPORTED GLOBAL)
add_dependencies(tree_sitter_cpp build_tree_sitter_cpp)
set_target_properties(tree_sitter_cpp PROPERTIES IMPORTED_LOCATION
	${TS_CPP_LIB})
target_include_directories(tree_sitter_cpp INTERFACE ${TS_CPP_INCLUDE})

add_library(TreeSitter INTERFACE)
target_link_libraries(TreeSitter INTERFACE tree_sitter tree_sitter_cpp)
