function(add_to_coverage target)
	if(ENABLE_COVERAGE)
		add_dependencies(build_all_coverage_targets ${target})
	endif()
endfunction()

set(COVERAGE_BACKEND "gcov" CACHE STRING "Coverage instrumentation: gcov or llvm (clang source-based)")
set_property(CACHE COVERAGE_BACKEND PROPERTY STRINGS gcov llvm)
set(COVERAGE_OPT_LEVEL "" CACHE STRING "Optimisation level of coverage builds (0-3); empty keeps the build type's own")

# Compile options come after the build type's flags, so this level wins.
if(ENABLE_COVERAGE AND NOT COVERAGE_OPT_LEVEL STREQUAL "")
	add_compile_options(-O${COVERAGE_OPT_LEVEL})
endif()

set(COVERAGE_EXCLUDED_PATHS
	"docs"
	"integration_tests"
	"scripts"
	"/tests/"
	"/playground/"
	"${CMAKE_BINARY_DIR}"
)

if(ENABLE_COVERAGE AND COVERAGE_BACKEND STREQUAL "gcov")
	# --coverage -fprofile-update=atomic is needed,
	# since the compiler, the VM and many tests are multithreaded.
	# -fprofile-exclude-files skips counters in system headers (std, LLVM): the coverage report
	# drops them anyway, and their shared atomic counters make the instrumented binaries much slower.
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-update=atomic")
	if(NOT BUILD_TYPE_IS_OPTIMISED AND COVERAGE_OPT_LEVEL STREQUAL "")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
	endif()
	if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		# ccache won't cache a compile with -fprofile-exclude-files on its command line, but it
		# hashes a specs file's contents. Specs files are GCC-only, so other compilers get the
		# flag directly and their coverage compiles are not cached.
		# Regexes are ;-separated, since | is a pipe in specs.
		set(COVERAGE_SPECS_FILE "${CMAKE_BINARY_DIR}/coverage.specs")
		file(CONFIGURE OUTPUT "${COVERAGE_SPECS_FILE}"
			CONTENT "*cc1plus:\n+ -fprofile-exclude-files=^/usr/;^${CMAKE_BINARY_DIR}/\n" @ONLY
		)
		add_compile_options("-specs=${COVERAGE_SPECS_FILE}")
	else()
		add_compile_options("-fprofile-exclude-files=^/usr/$<SEMICOLON>^${CMAKE_BINARY_DIR}/")
	endif()
	# Arc counters sit after calls, so no call is ever a tail call.
	set(COVERAGE_BLOCKS_TAIL_CALLS ON)

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
		--exclude ${COVERAGE_EXCLUDED_PATHS}
		--process-gcno # Process files not touched by tests (generated during compilation)
		--lcov # Generate output in lcov format
		-o coverage.info

		COMMAND ${GENHTML} --demangle-cpp -o coverage coverage.info --ignore-errors inconsistent

		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		VERBATIM
	)
	add_dependencies(coverage build_all_coverage_targets)
elseif(ENABLE_COVERAGE AND COVERAGE_BACKEND STREQUAL "llvm")
	if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		message(FATAL_ERROR "COVERAGE_BACKEND=llvm requires clang")
	endif()

	# Counters are inserted from the AST, so the report stays exact at any optimisation level.
	# %8m merges every run of a binary into at most 8 files, so many concurrent duckc processes
	# do not each leave a profile behind.
	set(COVERAGE_PROFILE_DIR "${CMAKE_BINARY_DIR}/profraw")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-generate=${COVERAGE_PROFILE_DIR}/%8m.profraw -fcoverage-mapping")

	string(REGEX MATCH "^[0-9]+" CLANG_MAJOR_VERSION "${CMAKE_CXX_COMPILER_VERSION}")

	# Single-byte counters only record whether a region ran, which is all the report needs, and
	# multithreaded code does not contend on them. Older clang leaves some regions without one
	# and reports executed code as not executed.
	set(COVERAGE_LLVM_SINGLE_BYTE_MIN_CLANG 23)
	if(CLANG_MAJOR_VERSION VERSION_GREATER_EQUAL COVERAGE_LLVM_SINGLE_BYTE_MIN_CLANG)
		option(COVERAGE_LLVM_SINGLE_BYTE "Use single-byte coverage counters" ON)
	else()
		option(COVERAGE_LLVM_SINGLE_BYTE "Use single-byte coverage counters" OFF)
	endif()

	if(COVERAGE_LLVM_SINGLE_BYTE)
		if(CLANG_MAJOR_VERSION VERSION_LESS COVERAGE_LLVM_SINGLE_BYTE_MIN_CLANG)
			message(FATAL_ERROR "COVERAGE_LLVM_SINGLE_BYTE needs clang ${COVERAGE_LLVM_SINGLE_BYTE_MIN_CLANG} or newer")
		endif()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mllvm -enable-single-byte-coverage=true")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-update=atomic")
	endif()
	add_link_options(-fprofile-instr-generate)

	get_filename_component(CLANG_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
	find_program(LLVM_PROFDATA NAMES llvm-profdata-${CLANG_MAJOR_VERSION} llvm-profdata HINTS ${CLANG_BIN_DIR} REQUIRED)
	find_program(LLVM_COV NAMES llvm-cov-${CLANG_MAJOR_VERSION} llvm-cov HINTS ${CLANG_BIN_DIR} REQUIRED)
	find_program(GENHTML genhtml REQUIRED)

	add_custom_target(build_all_coverage_targets)

	add_custom_target(coverage
		COMMAND /usr/bin/env python3 ${CMAKE_SOURCE_DIR}/scripts/cmake/llvm_coverage.py
		--llvm-profdata ${LLVM_PROFDATA}
		--llvm-cov ${LLVM_COV}
		--profile-dir ${COVERAGE_PROFILE_DIR}
		--build-dir ${CMAKE_BINARY_DIR}
		--exclude ${COVERAGE_EXCLUDED_PATHS}
		-o coverage.info

		COMMAND ${GENHTML} --demangle-cpp -o coverage coverage.info --ignore-errors inconsistent

		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		VERBATIM
	)
	add_dependencies(coverage build_all_coverage_targets)
elseif(ENABLE_COVERAGE)
	message(FATAL_ERROR "Unknown COVERAGE_BACKEND: ${COVERAGE_BACKEND}")
endif()

# The "--fair-sched=yes" option is used in VM debugger tests to speed up the tests,
# for more info see dev/src/vm/tests/api/debug/debugger_infinite_test.cpp
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
	# Every build pack drives the build tool in ${CMAKE_BINARY_DIR}. Two build
	# tools in one build tree race on the generated files and on the dependency
	# log, so ask ctest to run at most one build pack at a time.
	set_property(TEST ${BUILD_PACK_TARGET} PROPERTY RESOURCE_LOCK "duckling_build_tree")

	add_dependencies(build_all_tests ${BUILD_PACK_TARGET})
endfunction()
