cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
project(Duckling)

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

if(JIT_ENABLED)
    add_compile_definitions(ENABLE_JIT)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "JIT requires clang")
    endif()
endif()

# Dependencies
add_subdirectory(dependencies)

# Temp:
find_package(mimalloc 3.2 REQUIRED) 




# Save your strict flags
set(OLD_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
# Disable warnings for third-party
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
add_subdirectory(dependencies/abseil)
# Restore strict flags for your code
set(CMAKE_CXX_FLAGS "${OLD_CXX_FLAGS}")


# get_property(absl_targets DIRECTORY dependencies/abseil PROPERTY BUILDSYSTEM_TARGETS)
# foreach(t ${absl_targets})
#     message(STATUS "Abseil target: ${t}")
# endforeach()
# foreach(t ${absl_targets})
#     if(t MATCHES "^absl")
#         target_compile_options(${t} PRIVATE -Wno-error)
#     endif()
# endforeach()


include(${CMAKE_CURRENT_LIST_DIR}/main_cmake_files/ConfigureTests.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/main_cmake_files/ConfigurePlaygrounds.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/main_cmake_files/ConfigureGlobalCleanTargets.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/main_cmake_files/ConfigureModules.cmake)

# The first test is building tests.
add_custom_test_pack(base)
add_custom_test_pack(common)
add_custom_test_pack(compiler)


insertDependencies()

add_subdirectory(src/base)
add_subdirectory(src/common)
add_subdirectory(src/compiler)
add_subdirectory(src/vm)
add_subdirectory(src/playground)

# Scripts:
add_subdirectory(scripts/cmake)

include(${CMAKE_CURRENT_LIST_DIR}/main_cmake_files/ConfigureDocs.cmake)


# target_link_libraries(duckc PUBLIC mimalloc)
