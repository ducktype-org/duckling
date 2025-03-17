# Common functions:
add_custom_target(build_all_playgrounds)

function(duck_add_playground playground_base_name playground_user_source USES)
	if(${USES} STREQUAL "USES")
		set(playground_name ${playground_base_name}_playground)
		set(playground_source ${playground_base_name}_playground.cpp)

		if(${playground_source} STREQUAL ${playground_user_source})
			add_executable(${playground_name} ${CMAKE_CURRENT_LIST_DIR}/${playground_name}.cpp)
			target_link_libraries(${playground_name} ${ARGN})
			set_target_properties(${playground_name} PROPERTIES
				RUNTIME_OUTPUT_DIRECTORY ${PLAYGROUND_OUTPUT_DIRECTORY}
			)

			set_target_properties(${test_name} PROPERTIES EXCLUDE_FROM_ALL true)
			add_dependencies(build_all_playgrounds ${playground_name})
		else()
			message(FATAL_ERROR "Playground `${playground_base_name}` source file should be `${playground_source}` but is `${playground_user_source}`")
		endif()
	else()
		message(FATAL_ERROR "Test lacks uses clause")
	endif()
endfunction()
