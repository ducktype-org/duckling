function(add_to_coverage target)
	if(ENABLE_COVERAGE)
		add_dependencies(build_all_coverage_targets ${target})
	endif()
endfunction()

# The "--fair-sched=yes" option is used in VM debugger tests to speed up the tests,
# for more info see dev/VM/tests/debugger/debugger_infinite_tests.cpp
set(MEMORYCHECK_COMMAND_OPTIONS "--error-exitcode=1 --leak-check=full --fair-sched=yes")

# Coverage uses clang's source-based instrumentation. gcov wrote one .gcda per
# translation unit per process exit - 330 files and 17 MB for a single duckc run -
# which is what made coverage-instrumented integration tests slow. Clang writes one
# profile per process instead: an instrumented duckc run costs 37 ms rather than 188 ms.
if(ENABLE_COVERAGE)
	if(BUILD_TYPE_IS_OPTIMISED)
		message(WARNING "Chosen build type expects to be optimised, but coverage is enabled, which disables optimisations.")
	endif()

	if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		message(FATAL_ERROR
			"ENABLE_COVERAGE requires clang, not ${CMAKE_CXX_COMPILER_ID}. "
			"Re-run setup-build with a clang compiler, e.g. -x clang++-19 -c clang-19.")
	endif()

	# The profile runtime ships separately from the compiler (libclang-rt-<major>-dev
	# on Debian/Ubuntu). Without it everything configures and compiles happily and
	# then every single link fails, so fail here instead, where the message is useful.
	include(CheckCXXSourceCompiles)
	set(CMAKE_REQUIRED_FLAGS "-fprofile-instr-generate -fcoverage-mapping")
	check_cxx_source_compiles("int main() { return 0; }" HAVE_CLANG_PROFILE_RUNTIME)
	unset(CMAKE_REQUIRED_FLAGS)
	if(NOT HAVE_CLANG_PROFILE_RUNTIME)
		message(FATAL_ERROR
			"Cannot link with -fprofile-instr-generate: the clang profile runtime "
			"(libclang_rt.profile) is missing. On Debian/Ubuntu install "
			"libclang-rt-${CMAKE_CXX_COMPILER_VERSION_MAJOR}-dev.")
	endif()

	# llvm-profdata and llvm-cov read a .profraw format that is tied to the clang
	# that produced it, so they must come from the same LLVM as CMAKE_CXX_COMPILER
	# rather than from whatever happens to be first in PATH.
	get_filename_component(COVERAGE_LLVM_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
	find_program(LLVM_PROFDATA
		NAMES llvm-profdata-${CMAKE_CXX_COMPILER_VERSION_MAJOR} llvm-profdata
		HINTS "${COVERAGE_LLVM_BIN_DIR}"
		REQUIRED
	)
	find_program(LLVM_COV
		NAMES llvm-cov-${CMAKE_CXX_COMPILER_VERSION_MAJOR} llvm-cov
		HINTS "${COVERAGE_LLVM_BIN_DIR}"
		REQUIRED
	)
	find_program(GENHTML genhtml REQUIRED)

	# lcov >= 2.0 for `--filter`, which strips the non-code lines llvm-cov reports:
	#   blank - clang emits a skipped region for preprocessor-disabled code, and its
	#           bounds reach the blank lines and comments around the directive, so
	#           they land in the report at zero and drag the percentage down
	#   range - llvm-cov can name a line past the end of the file, which makes the
	#           tracefile invalid; see llvm/llvm-project#120071. It is also what makes
	#           `blank` work at all: without it lcov gives up on that file's source.
	# lcov also has a `brace` filter, which would drop the ~8k lines holding nothing
	# but a closing brace (gcov never counted those, llvm-cov does). It is left off
	# deliberately: it decides per line by looking at the neighbouring hit counts, so
	# it makes the unit and integration test denominators drift further apart than
	# they already do, and it is a bigger change to the metric than this is worth.
	find_program(LCOV lcov REQUIRED)
	execute_process(COMMAND ${LCOV} --version OUTPUT_VARIABLE LCOV_VERSION_OUTPUT
		ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
	string(REGEX MATCH "([0-9]+)\\.([0-9]+)" LCOV_VERSION "${LCOV_VERSION_OUTPUT}")
	if(LCOV_VERSION VERSION_LESS 2.0)
		message(FATAL_ERROR
			"Coverage needs lcov 2.0 or newer for --filter, found '${LCOV_VERSION}' "
			"(${LCOV_VERSION_OUTPUT}).")
	endif()
	set(COVERAGE_FILTERS "blank,range")

	message(STATUS "Coverage tools: ${LLVM_PROFDATA}, ${LLVM_COV}, lcov ${LCOV_VERSION}")

	set(COVERAGE_PROFILE_DIR "${CMAKE_BINARY_DIR}/profiles")
	set(COVERAGE_ITEST_PROFILE_DIR "${CMAKE_BINARY_DIR}/profiles-itests")
	file(MAKE_DIRECTORY "${COVERAGE_PROFILE_DIR}" "${COVERAGE_ITEST_PROFILE_DIR}")

	# Source files that are never interesting in a coverage report.
	set(COVERAGE_EXCLUDE_REGEX "(^${CMAKE_BINARY_DIR}/|/docs/|/integration_tests/|/scripts/|/tests/|/playground/|/_deps/|^/usr/)")

	# Every unit test writes its own profile, named after the test - see the
	# LLVM_PROFILE_FILE property set in duck_add_test_custom. A binary run without
	# that variable, which is how the integration tests invoke duckc and the VM,
	# falls back to the path compiled in here, so the integration test run needs
	# nothing prepared for it. `%8m` is an eight file merge pool: concurrent
	# processes merge into one of eight files instead of each writing its own.
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -fcoverage-mapping -fprofile-instr-generate=${COVERAGE_ITEST_PROFILE_DIR}/%8m.profraw")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -fcoverage-mapping -fprofile-instr-generate=${COVERAGE_ITEST_PROFILE_DIR}/%8m.profraw")

	add_custom_target(build_all_coverage_targets)

	# A pooled profile is merged into rather than overwritten, so without this a
	# second run reports the union of both. `toolbox.py coverage` runs it before
	# the tests; run it by hand if you drive ctest yourself.
	add_custom_target(coverage_reset
		COMMAND ${CMAKE_COMMAND} -E rm -rf ${COVERAGE_PROFILE_DIR} ${COVERAGE_ITEST_PROFILE_DIR}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_PROFILE_DIR} ${COVERAGE_ITEST_PROFILE_DIR}
		VERBATIM
	)

	# Unit tests: 171 executables, each with its own profile, collected in
	# parallel. See the script for why the pairing matters, and why it also
	# sweeps the build tree's object files.
	add_custom_target(coverage
		COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/cmake/llvm_coverage.py
			--binary-dir ${CMAKE_BINARY_DIR}/bin/tests
			--profile-dir ${COVERAGE_PROFILE_DIR}
			--baseline-dir ${CMAKE_BINARY_DIR}
			--output coverage-raw.info
			--llvm-profdata ${LLVM_PROFDATA}
			--llvm-cov ${LLVM_COV}
			--ignore-filename-regex "${COVERAGE_EXCLUDE_REGEX}"

		COMMAND ${LCOV} --filter ${COVERAGE_FILTERS}
			-a coverage-raw.info -o coverage.info

		COMMAND ${GENHTML} --demangle-cpp -o coverage coverage.info
			--ignore-errors inconsistent,unmapped

		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		VERBATIM
	)
	add_dependencies(coverage build_all_coverage_targets)

	# Integration tests: duckc and the VM are the only instrumented binaries they
	# run, and both share one profile pool, so a single llvm-cov is enough here.
	# The baseline sweep still applies, for the same reason as above.
	add_custom_target(coverage_itests
		COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/cmake/llvm_coverage.py
			--binary-dir ${CMAKE_BINARY_DIR}/bin
			--profile-dir ${COVERAGE_ITEST_PROFILE_DIR}
			--pooled
			--baseline-dir ${CMAKE_BINARY_DIR}
			--output coverage-itests-raw.info
			--llvm-profdata ${LLVM_PROFDATA}
			--llvm-cov ${LLVM_COV}
			--ignore-filename-regex "${COVERAGE_EXCLUDE_REGEX}"

		COMMAND ${LCOV} --filter ${COVERAGE_FILTERS}
			-a coverage-itests-raw.info -o coverage-itests.info

		COMMAND ${GENHTML} --demangle-cpp -o coverage-itests coverage-itests.info
			--ignore-errors inconsistent,unmapped

		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		VERBATIM
	)
endif()

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

	# `%p` keeps a test that forks instrumented children from having the children
	# overwrite its profile; llvm_coverage.py globs all of a test's files back together.
	if(ENABLE_COVERAGE)
		set_property(TEST "${test_name}" APPEND PROPERTY ENVIRONMENT
			"LLVM_PROFILE_FILE=${COVERAGE_PROFILE_DIR}/${test_name}.%p.profraw")
	endif()

	set_target_properties(${test_name} PROPERTIES EXCLUDE_FROM_ALL true)
	add_to_coverage(${test_name})

	if(JIT_ENABLED)
		string(FIND "${test_name}" "debugger" vm_tc_pos)
		if (vm_tc_pos EQUAL 6)
            set_property(TEST ${test_name} PROPERTY DISABLED TRUE)
		endif()
	endif()
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
