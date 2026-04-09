cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
project(Duckling)

option(USE_REPLXX "Use replxx library for REPL frontend" ON)

set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

# macOS platform detection and SDK configuration
if(APPLE)
	message("-- Configuring for macOS")
	# Find and set the macOS SDK path
	execute_process(
		COMMAND xcrun --show-sdk-path
		OUTPUT_VARIABLE MACOS_SDK_PATH
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET
	)
	if(MACOS_SDK_PATH)
		set(CMAKE_OSX_SYSROOT "${MACOS_SDK_PATH}" CACHE PATH "macOS SDK path" FORCE)
		message("-- macOS SDK path: ${CMAKE_OSX_SYSROOT}")
	else()
		message(WARNING "Could not find macOS SDK path using xcrun")
	endif()

	# On macOS with LLVM/Clang, prevent CMake from adding system include directories
	# that conflict with LLVM's libc++. The compiler already knows about the SDK
	# through CMAKE_OSX_SYSROOT and adding them explicitly causes header conflicts.
	if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
		# Clear implicit system include directories to prevent -isystem flags
		# for /usr/include and other system paths
		set(CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES "")
		set(CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES "")
		message("-- Cleared implicit include directories for LLVM/Clang compatibility")
	endif()
endif()

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

# Configure assembly targets generation (*.s) to include Intel-style asm and source code/asm interleaving.
# -masm=intel is GCC-only and not supported by Clang/AppleClang.
set(ASM_INTEL_FLAG "")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set(ASM_INTEL_FLAG "-masm=intel ")
endif()
set(CMAKE_CXX_CREATE_ASSEMBLY_SOURCE "<CMAKE_CXX_COMPILER> $(CXX_DEFINES) $(CXX_INCLUDES) ${CMAKE_CXX_FLAGS} -S ${ASM_INTEL_FLAG}-fverbose-asm <SOURCE> -o <ASSEMBLY_SOURCE> -std=c++23")

if(JIT_ENABLED)
    add_compile_definitions(ENABLE_JIT)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "JIT requires clang")
    endif()
endif()

# Dependencies
add_subdirectory(dependencies)

# On macOS with LLVM/Clang, sanitize all imported target include directories
# to remove problematic system paths that cause header conflicts with libc++
if(APPLE AND (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"))
	function(sanitize_target_includes target)
		if(TARGET ${target})
			get_target_property(target_type ${target} TYPE)
			if(NOT target_type STREQUAL "INTERFACE_LIBRARY")
				foreach(prop INTERFACE_INCLUDE_DIRECTORIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES INCLUDE_DIRECTORIES SYSTEM_INCLUDE_DIRECTORIES)
					get_target_property(includes ${target} ${prop})
					if(includes)
                        message("Sanitizing include directories for target ${target} (${prop}): ${includes}")
						list(FILTER includes EXCLUDE REGEX ".*/MacOSX\\.sdk/usr/include$")
						list(FILTER includes EXCLUDE REGEX "^/opt/homebrew/include$")
						list(FILTER includes EXCLUDE REGEX "^/usr/include$")
						list(FILTER includes EXCLUDE REGEX "^/usr/local/include$")
                        message("Sanitized include directories for target ${target} (${prop}): ${includes}")
						set_target_properties(${target} PROPERTIES ${prop} "${includes}")
					endif()
				endforeach()
			else()
				foreach(prop INTERFACE_INCLUDE_DIRECTORIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES)
					get_target_property(includes ${target} ${prop})
					if(includes)
                        message("Sanitizing include directories for INTERFACE target ${target} (${prop}): ${includes}")
						list(FILTER includes EXCLUDE REGEX ".*/MacOSX\\.sdk/usr/include$")
						list(FILTER includes EXCLUDE REGEX "^/opt/homebrew/include$")
						list(FILTER includes EXCLUDE REGEX "^/usr/include$")
						list(FILTER includes EXCLUDE REGEX "^/usr/local/include$")
                        message("Sanitized include directories for INTERFACE target ${target} (${prop}): ${includes}")
						set_target_properties(${target} PROPERTIES ${prop} "${includes}")
					endif()
				endforeach()
			endif()
		endif()
	endfunction()

	# Apply sanitization multiple times to catch all dependencies
	# This is needed because FetchContent creates targets that may not be visible
	# in the dependencies directory's BUILDSYSTEM_TARGETS
	macro(sanitize_all_targets)
		# Get all targets from the entire project
		get_property(all_project_targets DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY BUILDSYSTEM_TARGETS)
		get_property(all_imported_targets DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY IMPORTED_TARGETS)

		foreach(target ${all_project_targets} ${all_imported_targets})
			sanitize_target_includes(${target})
		endforeach()
	endmacro()

	# Sanitize after loading dependencies
	sanitize_all_targets()
	message("-- Sanitized include paths for all targets after dependencies")
endif()

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

# Final sanitization pass for macOS to ensure all targets are cleaned
if(APPLE AND (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"))
	sanitize_all_targets()
	message("-- Final sanitization pass completed")
endif()
