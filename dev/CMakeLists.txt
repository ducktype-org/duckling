cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
project(Duckling VERSION 0.2)

set(DUCKLING_VERSION_PRERELEASE "alpha")

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(USE_REPLXX "Use replxx library for REPL frontend" ON)

set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_COLOR_MAKEFILE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_LINK_LIBRARIES_ONLY_TARGETS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(TEST_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tests)
set(PLAYGROUND_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/playgrounds)

set(SRC_PATH ${CMAKE_CURRENT_LIST_DIR})

include(${CMAKE_CURRENT_LIST_DIR}/main_cmake_files/ConfigureBuildTypes.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/main_cmake_files/ConfigureCCache.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/main_cmake_files/ConfigureCompileOptions.cmake)

# Configure assembly targets generation (*.s) to include Intel-style asm and source code/asm interleaving. std=c++20 is needed for some reason.
set(CMAKE_CXX_CREATE_ASSEMBLY_SOURCE "<CMAKE_CXX_COMPILER> $(CXX_DEFINES) $(CXX_INCLUDES) ${CMAKE_CXX_FLAGS} -S -masm=intel -fverbose-asm <SOURCE> -o <ASSEMBLY_SOURCE> -std=c++23")

option(JIT_ENABLED "Enable JIT compilation" OFF)
if(JIT_ENABLED)
    add_compile_definitions(ENABLE_JIT)
    if (CNP_ENABLED)
        add_compile_definitions(COMPILE_WITH_CNP=1)
    else()
        add_compile_definitions(COMPILE_WITH_CNP=0)
    endif()


    set(LLVM_TOOLS_ARGS
        --llvm-opt=${LLVM_OPT}
        --llvm-link=${LLVM_LINK}
        --llvm-nm=${LLVM_NM}
        --llvm-readobj=${LLVM_READOBJ}
        --llvm-cxxfilt=${LLVM_CXXFILT}
        --llvm-extract=${LLVM_EXTRACT}
    )

    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "JIT requires clang")
    endif()
endif()

# Dependencies
add_subdirectory(dependencies)

include(${CMAKE_CURRENT_LIST_DIR}/main_cmake_files/ConfigureTests.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/main_cmake_files/ConfigurePlaygrounds.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/main_cmake_files/ConfigureGlobalCleanTargets.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/main_cmake_files/ConfigureModules.cmake)

# The first test is building tests.
add_custom_test_pack(base)
add_custom_test_pack(common)
add_custom_test_pack(compiler)
add_custom_test_pack(tools)


insertDependencies()

add_subdirectory(src/base)
add_subdirectory(src/common)
add_subdirectory(src/compiler)
add_subdirectory(src/vm)
add_subdirectory(src/playground)
add_subdirectory(src/tools)

# Scripts:
add_subdirectory(scripts/cmake)

include(${CMAKE_CURRENT_LIST_DIR}/main_cmake_files/ConfigureDocs.cmake)
