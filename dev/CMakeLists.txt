cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(Rift)

cmake_policy(SET CMP0079 NEW)
cmake_policy(SET CMP0054 NEW)

# This is ifed-out, because it is
# avaiable only from CMAKE 3.24.
# @FUTURE: un-if it.
# Note: It is not that important for us (as of 11.2023).
#       It is realated to dependecies. 
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.24.0") 
	cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# @TODO: add -Werror=shadow=local, but "turn it off" for Crow and Asio
set(CMAKE_CXX_FLAGS "-Werror=return-type -Werror=terminate -Wshadow=local -Wall -Wextra -Wno-sign-compare -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_CREATE_ASSEMBLY_SOURCE "<CMAKE_CXX_COMPILER> $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -O2 -g -S -masm=intel -fverbose-asm <SOURCE> -o <ASSEMBLY_SOURCE>")

if (ENABLE_COVERAGE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -coverage")
	find_program(LCOV lcov REQUIRED)
	find_program(GENHTML genhtml REQUIRED)

	add_custom_target(coverage
			COMMAND ${LCOV} --directory . --capture --output-file coverage.info
			COMMAND ${GENHTML} --demangle-cpp -o coverage coverage.info
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif ()

set(MEMORYCHECK_COMMAND_OPTIONS "--error-exitcode=1 --leak-check=full")
include(CTest)
enable_testing()

set(SRC_PATH ${CMAKE_CURRENT_LIST_DIR})

# if(NDEBUG)
# set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
# else()
# set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -O3)
# endif()

# Common functions:
function(rift_add_test test_pack test_name source USES)
	if (${USES} STREQUAL "USES")
		add_executable(${test_name} ${CMAKE_CURRENT_LIST_DIR}/${source})
		target_link_libraries(${test_name} Tester ${ARGN})

		add_test(NAME "${test_name}" COMMAND ${test_name} ${CMAKE_SOURCE_DIR})

		# It is needed in case tests are run on multiple threads.
		set_target_properties(${test_name} PROPERTIES DEPENDS build_${test_pack}_tests)
		add_dependencies(build_${test_pack}_tests ${test_name})
		set_property(TEST "${test_name}" PROPERTY LABELS "${test_pack}")

		set_target_properties(${test_name} PROPERTIES EXCLUDE_FROM_ALL true)
	else ()
		message(FATAL_ERROR "rift_add_test lacks uses clause")
	endif ()
endfunction()

function(make_module MODULE_NAME USES)
	target_include_directories(${MODULE_NAME} PUBLIC src)

	if (${USES} STREQUAL "USES")
		target_link_libraries(${MODULE_NAME} ${ARGN}) # ARGN represents all arguemtns beyond defined
	else ()
		message(FATAL_ERROR "make_module lacks uses clause")
	endif ()
endfunction()

function(add_example exmaple_name source USES)
	if (${USES} STREQUAL "USES")
		add_executable(${exmaple_name} ${source})
		target_link_libraries(${exmaple_name} ${ARGN})
		set_target_properties(${exmaple_name} PROPERTIES EXCLUDE_FROM_ALL true)
	else ()
		message(FATAL_ERROR "add_example lacks uses clause")
	endif ()
endfunction()

add_custom_target(build_all_tests)

function(add_custom_test_pack NAME)
	set(BUILD_PACK_TARGET "build_${NAME}_tests")
	add_custom_target("${BUILD_PACK_TARGET}")
	add_test(NAME ${BUILD_PACK_TARGET} COMMAND "${CMAKE_COMMAND}" --build ${CMAKE_BINARY_DIR} --target ${BUILD_PACK_TARGET})
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

if (BUILD_DOCS)
	add_subdirectory(docs)
endif (BUILD_DOCS)
