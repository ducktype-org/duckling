function(add_to_coverage target)
	if(ENABLE_COVERAGE)
		add_dependencies(build_all_coverage_targets ${target})
	endif()
endfunction()

if (ENABLE_COVERAGE AND BUILD_TYPE_IS_OPTIMISED)
	message(WARNING "Chosen build type expects to be optimised, but coverage is enabled, which disables optimisations.")
endif()

if(ENABLE_COVERAGE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 --coverage")

	find_program(FASTCOV fastcov REQUIRED)

	find_program(LCOV lcov REQUIRED)

	if(NOT GCOV_VERSION)
		message(FATAL_ERROR "GCOV_VERSION is not set.")
	endif()

	find_program(GCOV_PATH ${GCOV_VERSION} REQUIRED)

	message(STATUS "GCOV path: ${GCOV_PATH}")

	find_program(GENHTML genhtml REQUIRED)

	add_custom_target(build_all_coverage_targets)

	add_custom_target(coverage
		COMMAND ${FASTCOV}
		--gcov ${GCOV_VERSION}
		--include "${CMAKE_SOURCE_DIR}" # Process only files in this folder
		--exclude "docs" # Exclude paths containing this name from coverage
		"integration_tests"
		"scripts"
		"/tests/"
		"/playground/"
		"${CMAKE_BINARY_DIR}" # Exclude paths containing this name from coverage
		--process-gcno # Process files not touched by tests (generated during compilation)
		--lcov # Generate output in lcov format
		-o coverage.info

		COMMAND ${GENHTML} --demangle-cpp -o coverage coverage.info --ignore-errors inconsistent

		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		VERBATIM
	)
	add_dependencies(coverage build_all_coverage_targets)
endif()

# The "--fair-sched=yes" option is used in VM debugger tests to speed up the tests,
# for more info see dev/VM/tests/debugger/debugger_infinite_tests.cpp
set(MEMORYCHECK_COMMAND_OPTIONS "--error-exitcode=1 --leak-check=full --fair-sched=yes")

include(CTest)
enable_testing()

# Common functions:
function(duck_add_test_custom test_pack test_name test_source)
	set(multiValueArgs USES INCLUDE)
	cmake_parse_arguments(duck_add_test_custom "" "" "${multiValueArgs}" ${ARGN})

	# Checks if USES is used with 0 or more arguments
	if((NOT duck_add_test_custom_USES) AND(NOT("USES" IN_LIST duck_add_test_custom_KEYWORDS_MISSING_VALUES)))
		message(FATAL_ERROR "duck_add_test_custom lacks uses argument")
	endif()

	add_executable(${test_name} ${CMAKE_CURRENT_LIST_DIR}/${test_source})
	target_link_libraries(${test_name} Tester ${duck_add_test_custom_USES})
	target_include_directories(${test_name} PUBLIC ${duck_add_test_custom_INCLUDE})

	# Some symbols that are necessary for the JIT to work have to be looked up in the executor process's
	# dynamic symbol table, and this flag makes all symbols exported to the dynamic symbol table.
	# This also applies to test suites, as they introduce additional symbols.
	if(JIT_ENABLED)
		target_link_options(${test_name} PRIVATE -Wl,--export-dynamic)
	endif()

	add_test(NAME "${test_name}" COMMAND ${test_name})

	# It is needed in case tests are run on multiple threads.
	set_target_properties(${test_name} PROPERTIES DEPENDS build_${test_pack}_tests)
	set_target_properties(${test_name} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY ${TEST_OUTPUT_DIRECTORY}
	)
	add_dependencies(build_${test_pack}_tests ${test_name})
	set_property(TEST "${test_name}" PROPERTY LABELS "${test_pack}")

	set_target_properties(${test_name} PROPERTIES EXCLUDE_FROM_ALL true)
	add_to_coverage(${test_name})
endfunction()

function(duck_add_test test_pack test_base_name test_user_source)
	set(test_name ${test_base_name}_test)
	set(test_source tests/${test_name}.cpp)

	if(${test_source} STREQUAL ${test_user_source})
		duck_add_test_custom(${test_pack} ${test_name} ${test_source} ${ARGN})
	else()
		message(FATAL_ERROR "Test `${test_base_name}` source file should be `${test_source}` but is `${test_user_source}`")
	endif()
endfunction()

add_custom_target(build_all_tests)

function(add_custom_test_pack NAME)
	set(BUILD_PACK_TARGET "build_${NAME}_tests")
	add_custom_target("${BUILD_PACK_TARGET}")
	add_test(NAME ${BUILD_PACK_TARGET} COMMAND "${CMAKE_COMMAND}" --build ${CMAKE_BINARY_DIR} --target ${BUILD_PACK_TARGET} -j ${CMAKE_BUILD_PARALLEL_LEVEL})
	set_property(TEST ${BUILD_PACK_TARGET} PROPERTY LABELS "${NAME}")

	add_dependencies(build_all_tests ${BUILD_PACK_TARGET})
endfunction()
