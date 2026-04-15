# Function for creating a module from cmake library.
# It should be used to setup compilation of (almost) all of cpp source files.
#
# Usage: make_module(MODULE_NAME USES <module1> <module2> ...] [INCLUDE <include1> <include2> ...])
# * MODULE_NAME should be the same as a name of library that hold module source files.
# * USES is a list of libraries that the module depends on. It should be used with 0 or more arguments.
# * INCLUDE is a list of include directories that the module depends on. It should be used with 0 or more arguments.
#
# make_module function assumes that there are two folders present in the source directory:
# * src - contains the source files of the module
# * src_private - contains the private source files of the module
#
# When module X uses module Y, it means that:
# * Module Y will be linked to module X
# * Module Y src folder will be in include paths of module X
#
# Both of the above are transitive, meaning that if X uses Y and Y uses Z, then it behaves as if X uses Z
#
function(make_module MODULE_NAME)
	set(multiValueArgs USES INCLUDE)
	cmake_parse_arguments(make_module "" "" "${multiValueArgs}" ${ARGN})

	# Checks if USES is used with 0 or more arguments
	if((NOT make_module_USES) AND(NOT("USES" IN_LIST make_module_KEYWORDS_MISSING_VALUES)))
		message(FATAL_ERROR "make_module lacks uses argument")
	endif()

	set_property(TARGET clean-modules APPEND PROPERTY MODULE_DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR})

	target_include_directories(${MODULE_NAME} PUBLIC src ${make_module_INCLUDE})
	target_include_directories(${MODULE_NAME} PRIVATE src_private)
	target_link_libraries(${MODULE_NAME} ${make_module_USES})
	add_to_coverage(${MODULE_NAME})
endfunction()

add_custom_target(build_all_examples)

function(add_example example_name source USES)
	if(${USES} STREQUAL "USES")
		add_executable(${example_name} ${source})
		target_link_libraries(${example_name} ${ARGN})
		set_target_properties(${example_name} PROPERTIES EXCLUDE_FROM_ALL true)
		add_dependencies(build_all_examples ${example_name})
	else()
		message(FATAL_ERROR "add_example lacks uses clause")
	endif()
endfunction()
