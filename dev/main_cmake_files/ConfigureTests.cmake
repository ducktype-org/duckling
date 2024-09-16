set(GCOV_PATH "gcov" CACHE STRING "GCOV program path")
if(ENABLE_COVERAGE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 --coverage")
	find_program(LCOV lcov REQUIRED)
	find_program(GENHTML genhtml REQUIRED)

	add_custom_target(coverage
		COMMAND ${LCOV} --directory "${CMAKE_SOURCE_DIR}" --capture --output-file coverage.info
			--base-directory "${CMAKE_SOURCE_DIR}" --no-external --exclude "**/_deps/**" --gcov-tool ${GCOV_PATH}
		COMMAND ${GENHTML} --demangle-cpp -o coverage coverage.info
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		VERBATIM)
endif()


set(MEMORYCHECK_COMMAND_OPTIONS "--error-exitcode=1 --leak-check=full")

include(CTest)
enable_testing()

# Common functions:
function(rift_add_test test_pack test_name source USES)
	if(${USES} STREQUAL "USES")
		add_executable(${test_name} ${CMAKE_CURRENT_LIST_DIR}/${source})
		target_link_libraries(${test_name} Tester ${ARGN})

		add_test(NAME "${test_name}" COMMAND ${test_name})

		# It is needed in case tests are run on multiple threads.
		set_target_properties(${test_name} PROPERTIES DEPENDS build_${test_pack}_tests)
		add_dependencies(build_${test_pack}_tests ${test_name})
		set_property(TEST "${test_name}" PROPERTY LABELS "${test_pack}")

		# This adds LD_LIBRARY_PATH pointing to downloaded ICU when using DOWNLOAD_UBUNTU_ICU_BUILD.
		# In general it should be only used in workflows.
		if(DOWNLOAD_UBUNTU_ICU_BUILD STREQUAL "ON")
			set_property(TEST "${test_name}"
				PROPERTY ENVIRONMENT
				"LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/_deps/ubuntu-icu-src/usr/local/lib/")
		endif()

		set_target_properties(${test_name} PROPERTIES EXCLUDE_FROM_ALL true)
	else()
		message(FATAL_ERROR "rift_add_test lacks uses clause")
	endif()
endfunction()

add_custom_target(build_all_tests)

function(add_custom_test_pack NAME)
	set(BUILD_PACK_TARGET "build_${NAME}_tests")
	add_custom_target("${BUILD_PACK_TARGET}")
	add_test(NAME ${BUILD_PACK_TARGET} COMMAND "${CMAKE_COMMAND}" --build ${CMAKE_BINARY_DIR} --target ${BUILD_PACK_TARGET} -j ${CMAKE_BUILD_PARALLEL_LEVEL})
	set_property(TEST ${BUILD_PACK_TARGET} PROPERTY LABELS "${NAME}")

	add_custom_target("test_${NAME}"
		COMMAND ${CMAKE_CTEST_COMMAND} -L ${NAME}
		WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

	add_custom_target("memcheck_test_${NAME}"
		COMMAND ${CMAKE_CTEST_COMMAND} -L ${NAME}
		--force-new-ctest-process --test-action memcheck
		WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

	add_dependencies(build_all_tests ${BUILD_PACK_TARGET})

	set_target_properties("test_${NAME}" PROPERTIES EXCLUDE_FROM_ALL true)
endfunction()

add_custom_target(memcheck_test
	COMMAND ${CMAKE_CTEST_COMMAND}
	--force-new-ctest-process --test-action memcheck
	WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
