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

function(add_example exmaple_name source USES)
	if(${USES} STREQUAL "USES")
		add_executable(${exmaple_name} ${source})
		target_link_libraries(${exmaple_name} ${ARGN})
		set_target_properties(${exmaple_name} PROPERTIES EXCLUDE_FROM_ALL true)
	else()
		message(FATAL_ERROR "add_example lacks uses clause")
	endif()
endfunction()
