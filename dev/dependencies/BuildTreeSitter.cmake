# Downloads and sets up tree sitter parser for usage with c++

include(FetchContent)

function(BuildTreeSitter)
	set(oneValueArgs TS_URL TS_CPP_URL)
	cmake_parse_arguments(BuildTreeSitter "" "${oneValueArgs}" "" ${ARGN})
 
	set(DEPS_DIR ${PROJECT_BINARY_DIR}/_deps)

	set(TS_DIR ${DEPS_DIR}/tree-sitter-src)
	set(TS_CPP_DIR ${DEPS_DIR}/tree-sitter-cpp-src)

	set(TS_LIB ${TS_DIR}/libtree-sitter.a)
	set(TS_CPP_LIB ${TS_CPP_DIR}/libtree-sitter-cpp.a)

	set(TS_INCLUDE ${TS_DIR}/lib/include)
	set(TS_CPP_INCLUDE ${TS_CPP_DIR}/bindings/c)

	FetchContent_Declare(
		fetch_tree_sitter
		URL ${BuildTreeSitter_TS_URL}
		SOURCE_DIR ${TS_DIR}
	)

	FetchContent_Declare(
		fetch_tree_sitter_cpp
		URL ${BuildTreeSitter_TS_CPP_URL}
		SOURCE_DIR ${TS_CPP_DIR}
	)

	FetchContent_MakeAvailable(fetch_tree_sitter fetch_tree_sitter_cpp)

	add_custom_target(
		build_tree_sitter
		BYPRODUCTS ${TS_LIB}
		COMMAND make -j --quiet
		WORKING_DIRECTORY ${TS_DIR}
		VERBATIM
	)

	add_custom_target(
		build_tree_sitter_cpp
		BYPRODUCTS ${TS_CPP_LIB}
		COMMAND make -j --quiet
		WORKING_DIRECTORY ${TS_CPP_DIR}
		VERBATIM
	)

	add_library(tree_sitter STATIC IMPORTED GLOBAL)
	add_dependencies(tree_sitter build_tree_sitter)
	set_target_properties(tree_sitter PROPERTIES IMPORTED_LOCATION
		${TS_LIB})
	target_include_directories(tree_sitter INTERFACE ${TS_INCLUDE})

	add_library(tree_sitter_cpp STATIC IMPORTED GLOBAL)
	add_dependencies(tree_sitter_cpp build_tree_sitter_cpp)
	set_target_properties(tree_sitter_cpp PROPERTIES IMPORTED_LOCATION
		${TS_CPP_LIB})
	target_include_directories(tree_sitter_cpp INTERFACE ${TS_CPP_INCLUDE})

	add_library(TreeSitter INTERFACE)
	target_link_libraries(TreeSitter INTERFACE tree_sitter tree_sitter_cpp)

endfunction()
