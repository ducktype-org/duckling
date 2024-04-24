cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(Rift)

cmake_policy(SET CMP0079 NEW)
cmake_policy(SET CMP0054 NEW)

set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

# This is ifed-out, because it is
# avaiable only from CMAKE 3.24.
# @FUTURE: un-if it.
# Note: It is not that important for us (as of 11.2023).
# It is realated to dependecies.
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.24.0")
	cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_COLOR_MAKEFILE ON)

option(USE_CCACHE "Use ccache" OFF)

if(USE_CCACHE)
	message(NOTICE "Ccache might not work with generators other then make or ninja!")

	find_program(CCACHE "ccache" PATHS "scripts/downloads/" NO_DEFAULT_PATH REQUIRED)

	if(CCACHE)
		message(STATUS "Ccache set to: ${CCACHE}")

		# As of 02.2024 there is no better way to set configuration other then listing it here:
		# @FUTURE It might change in the future
		set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE};cache_dir=${CMAKE_CURRENT_SOURCE_DIR}/.ccache_cache/;max_size=1")
	else()
		message(ERROR "CCACHE not found.")
	endif(CCACHE)
endif(USE_CCACHE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	# @TODO: decide of std++20 vs gnu++20
	message("-- GNU compiler")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-type -Werror=terminate -Werror=shadow=local -Werror=return-local-addr -Werror=free-nonheap-object -Wall -Wextra -Wno-sign-compare")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	message("-- Clang compiler")

	# I didn't find a good -Werror=terminate alternative for Clang.
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-type -Werror=shadow-all -Werror=return-stack-address -Werror=free-nonheap-object -Wno-shadow-field-in-constructor -Wno-shadow-field -Wall -Wextra -Wno-sign-compare")
else()
	message(FATAL_ERROR "Error: UNKNOWN COMPILER")
endif()

# "-O2" here is needed so standard "cmake .." is compiled with O2.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

# Debug version uses O0 and prints all logs.
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DPRINT_LOG -g -Og")

# Release version uses O2, not O3. It might change.
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Configure assembly targets generation (*.s) to include Intel-style asm and source code/asm interleaving. std=c++20 is needed for some reason.
set(CMAKE_CXX_CREATE_ASSEMBLY_SOURCE "<CMAKE_CXX_COMPILER> $(CXX_DEFINES) $(CXX_INCLUDES) ${CMAKE_CXX_FLAGS} -S -masm=intel -fverbose-asm <SOURCE> -o <ASSEMBLY_SOURCE> -std=c++20")

if(ENABLE_COVERAGE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 --coverage")
	find_program(LCOV lcov REQUIRED)
	find_program(GENHTML genhtml REQUIRED)

	add_custom_target(coverage
		COMMAND ${LCOV} --directory . --capture --output-file coverage.info
		COMMAND ${GENHTML} --demangle-cpp -o coverage coverage.info
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

set(MEMORYCHECK_COMMAND_OPTIONS "--error-exitcode=1 --leak-check=full")
include(CTest)
enable_testing()

set(SRC_PATH ${CMAKE_CURRENT_LIST_DIR})

# Common functions:
function(rift_add_test test_pack test_name source USES)
	if(${USES} STREQUAL "USES")
		add_executable(${test_name} ${CMAKE_CURRENT_LIST_DIR}/${source})
		target_link_libraries(${test_name} Tester ${ARGN})

		add_test(NAME "${test_name}" COMMAND ${test_name} ${CMAKE_SOURCE_DIR})

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

define_property(TARGET PROPERTY MODULE_DIRECTORIES BRIEF_DOCS "Used for clean-modules target" FULL_DOCS "Used for clean-modules target")
add_custom_target(clean-modules
	echo "Deleting following directories:"
	COMMAND echo -e "$<JOIN:$<TARGET_PROPERTY:clean-modules,MODULE_DIRECTORIES>,\\n>"
	COMMAND rm -rfI "$<TARGET_PROPERTY:clean-modules,MODULE_DIRECTORIES>" || "true"
	USES_TERMINAL
	COMMAND_EXPAND_LISTS
	VERBATIM)

function(make_module MODULE_NAME)
	set(multiValueArgs USES INCLUDE)
	cmake_parse_arguments(make_module "" "" "${multiValueArgs}" ${ARGN})

	# Checks if USES is used with 0 or more arguments
	if((NOT make_module_USES) AND(NOT("USES" IN_LIST make_module_KEYWORDS_MISSING_VALUES)))
		message(FATAL_ERROR "make_module lacks uses argument")
	endif()

	set_property(TARGET clean-modules APPEND PROPERTY MODULE_DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR})

	target_include_directories(${MODULE_NAME} PUBLIC src ${make_module_INCLUDE})
	target_link_libraries(${MODULE_NAME} ${make_module_USES})
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

# Dependencies
add_subdirectory(dependencies)

# The first test is building tests.
add_custom_test_pack(base)
add_custom_test_pack(common)
add_custom_test_pack(compiler)

add_subdirectory(base)
add_subdirectory(common)
add_subdirectory(RiftCompiler)
add_subdirectory(RiftVM)
add_subdirectory(playground)

add_custom_target(memcheck_test
	COMMAND ${CMAKE_CTEST_COMMAND}
	--force-new-ctest-process --test-action memcheck
	WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

add_subdirectory(scripts/cmake)

# We still want to compile project,
# if docs dependencies are not present (e.g. in GitHub workflows).
# Add `-D BUILD_DOCS=OFF` option to CMake to ignore docs
option(BUILD_DOCS "Build test programs" ON)

if(BUILD_DOCS)
	add_subdirectory(docs)

	# add_custom_target(docs-clean make -s -f CmakeFiles/Makefile2 docs/clean)
	# add_custom_target(docs-clean ninja -t clean docs)
endif(BUILD_DOCS)
