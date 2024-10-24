cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
project(Duckling)

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
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(SRC_PATH ${CMAKE_CURRENT_LIST_DIR})

include(${CMAKE_CURRENT_LIST_DIR}/main_cmake_files/ConfigureCCache.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/main_cmake_files/ConfigureCompileOptions.cmake)

# Configure assembly targets generation (*.s) to include Intel-style asm and source code/asm interleaving. std=c++20 is needed for some reason.
set(CMAKE_CXX_CREATE_ASSEMBLY_SOURCE "<CMAKE_CXX_COMPILER> $(CXX_DEFINES) $(CXX_INCLUDES) ${CMAKE_CXX_FLAGS} -S -masm=intel -fverbose-asm <SOURCE> -o <ASSEMBLY_SOURCE> -std=c++20")

include(${CMAKE_CURRENT_LIST_DIR}/main_cmake_files/ConfigureTests.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/main_cmake_files/ConfigureGlobalCleanTargets.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/main_cmake_files/ConfigureModules.cmake)

# The first test is building tests.
add_custom_test_pack(base)
add_custom_test_pack(common)
add_custom_test_pack(compiler)

# Dependencies
add_subdirectory(dependencies)
insertLLVM()

# Source:
add_subdirectory(base)
add_subdirectory(common)
add_subdirectory(compiler)
add_subdirectory(VM)
add_subdirectory(playground)

# Scripts:
add_subdirectory(scripts/cmake)

include(${CMAKE_CURRENT_LIST_DIR}/main_cmake_files/ConfigureDocs.cmake)
