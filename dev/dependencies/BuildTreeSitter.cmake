# Downloads and sets up tree sitter parser for usage with c++

include(ExternalProject)

function(BuildTreeSitter)
	set(oneValueArgs TS_URL TS_CPP_URL)
	cmake_parse_arguments(BuildTreeSitter "" "${oneValueArgs}" "" ${ARGN})
	
	set(DEPS_DIR ${PROJECT_BINARY_DIR}/_deps)
	set(DOWNLOAD_DIR ${DEPS_DIR}/download)

	set(TS_DIR ${DEPS_DIR}/tree-sitter-src)
	set(TS_CPP_DIR ${DEPS_DIR}/tree-sitter-cpp-src)

	set(TS_LIB ${TS_DIR}/libtree-sitter.a)
	set(TS_CPP_LIB ${TS_CPP_DIR}/libtree-sitter-cpp.a)

	set(TS_INCLUDE ${TS_DIR}/lib/include)
	set(TS_CPP_INCLUDE ${TS_CPP_DIR}/bindings/c)

	ExternalProject_Add(
		external_tree_sitter
		DOWNLOAD_DIR ${DOWNLOAD_DIR}
		URL ${BuildTreeSitter_TS_URL}
		SOURCE_DIR ${TS_DIR}
		BUILD_IN_SOURCE TRUE
		CONFIGURE_COMMAND ""
		BUILD_COMMAND "make"
		BUILD_BYPRODUCTS ${TS_LIB}
		INSTALL_COMMAND ""
	)

	ExternalProject_Add(
		external_tree_sitter_cpp
		DOWNLOAD_DIR ${DOWNLOAD_DIR}
		URL ${BuildTreeSitter_TS_CPP_URL}
		SOURCE_DIR ${TS_CPP_DIR}
		BUILD_IN_SOURCE TRUE
		CONFIGURE_COMMAND ""
		BUILD_COMMAND "make"
		BUILD_BYPRODUCTS ${TS_CPP_LIB}
		INSTALL_COMMAND ""
	)

	add_library(tree_sitter STATIC IMPORTED GLOBAL)
	set_target_properties(tree_sitter PROPERTIES IMPORTED_LOCATION
		${TS_LIB})
	add_dependencies(tree_sitter external_tree_sitter)
	target_include_directories(tree_sitter INTERFACE ${TS_INCLUDE})

	add_library(tree_sitter_cpp STATIC IMPORTED GLOBAL)
	set_target_properties(tree_sitter_cpp PROPERTIES IMPORTED_LOCATION
		${TS_CPP_LIB})
	add_dependencies(tree_sitter_cpp external_tree_sitter_cpp)
	target_include_directories(tree_sitter_cpp INTERFACE ${TS_CPP_INCLUDE})

	add_library(TreeSitter INTERFACE)
	target_link_libraries(TreeSitter INTERFACE tree_sitter tree_sitter_cpp)

endfunction()
