option(USE_MARCH_NATIVE "Use -march=native. This should be disabled for portable builds" OFF)
option(STRIP_SYMBOL_INFORMATION "Strip symbol information from binaries" OFF)
option(DISABLE_UNITY_COMPILATION "Disable unity builds" OFF)
option(ENABLE_LINK_TIME_OPTIMIZATION "Enable link time optimization" OFF)
set(SANITIZER "" CACHE STRING "Enable a sanitizer (ASAN, TSAN, UBSAN)")

# disable compiler-specific extensions
set(CMAKE_CXX_EXTENSIONS OFF)
# require compiler to support C++ standard it is asked for
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(IGNORE_COMPILER_VERSION_CHECK
	"Configure even if the C++ compiler is older than the oldest supported version" OFF)

# Stops the configuration when the C++ compiler is older than the oldest version we know builds the project.
function(check_minimum_compiler_version compiler_name minimum_version)
	if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS minimum_version)
		return()
	endif()

	string(CONCAT version_message
		"Your C++ compiler is too old to build Duckling.\n"
		"  Compiler path: ${CMAKE_CXX_COMPILER}\n"
		"  Found:         ${compiler_name} ${CMAKE_CXX_COMPILER_VERSION}\n"
		"  Required:      ${compiler_name} ${minimum_version} or newer\n"
		"If you are sure this compiler works, configure with "
		"-DIGNORE_COMPILER_VERSION_CHECK=ON to turn this error into a warning.")

	if (IGNORE_COMPILER_VERSION_CHECK)
		message(WARNING "${version_message}")
	else()
		message(FATAL_ERROR "${version_message}")
	endif()
endfunction()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	message("-- GNU compiler")

	check_minimum_compiler_version("GCC" 14)


	string(CONCAT ADDITIONAL_GNU_FLAGS
		"-Werror=return-type "
		"-Werror=terminate "
		"-Werror=shadow=local "
		"-Werror=return-local-addr "
		"-Werror=free-nonheap-object "
		"-Werror=conversion "
		"-Werror=implicit-fallthrough "
		"-Werror=missing-field-initializers "
		"-Werror=reorder "
		"-Werror=invalid-memory-model "
		"-Wall -Wextra "
		"-pedantic "
		"-Wno-sign-compare "
		"-Wno-redundant-move "
		)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ADDITIONAL_GNU_FLAGS}")

	if(APPLE)
		# The macOS SDK headers use the C keyword _Static_assert, which GCC rejects
		# in C++ mode; map it onto C++'s static_assert.
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_Static_assert=static_assert")
	endif()

	# Debug version uses O0.

elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
	message("-- ${CMAKE_CXX_COMPILER_ID} compiler")

	if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		check_minimum_compiler_version("Clang" 19)
	elseif (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
		check_minimum_compiler_version("Apple Clang" 16.3)
	else()
		message(FATAL_ERROR "Unsupported Clang compiler: ${CMAKE_CXX_COMPILER_ID}")
	endif()

	# I didn't find a good -Werror=terminate alternative for Clang.
	# The "-Werror=shadow" is more strict than "-Werror=shadow=local".
	string(CONCAT ADDITIONAL_CLANG_FLAGS
		"-Werror=return-type "
		"-Werror=return-stack-address "
		"-Werror=free-nonheap-object "
		"-Werror=conversion "
		"-Werror=implicit-fallthrough "
		"-Werror=missing-field-initializers "
		"-Wall -Wextra "
		"-pedantic "
		"-Wno-sign-compare "
		"-Wno-sign-conversion "
		"-Wno-redundant-move "
	)

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ADDITIONAL_CLANG_FLAGS}" )

	if(APPLE)
		# Homebrew's clang is not the Apple one, so it does not find the macOS SDK on
		# its own; point it at the SDK that xcrun reports.
		if(NOT CMAKE_OSX_SYSROOT)
			execute_process(COMMAND xcrun --show-sdk-path
					OUTPUT_VARIABLE MACOS_SDK_PATH
					OUTPUT_STRIP_TRAILING_WHITESPACE
					ERROR_QUIET)
			if(NOT MACOS_SDK_PATH)
				message(FATAL_ERROR
						"Could not determine the macOS SDK path. "
						"Install the command line tools with `xcode-select --install`.")
			endif()
			message("-- Using macOS SDK: ${MACOS_SDK_PATH}")
			set(CMAKE_OSX_SYSROOT "${MACOS_SDK_PATH}")
		endif()
	endif()

else()
	message(FATAL_ERROR "Error: UNKNOWN COMPILER")
endif()

if (USE_MARCH_NATIVE)
	add_compile_options(-march=native)
endif (USE_MARCH_NATIVE)


# Dev:       simple dev build without debug symbols and without optimizations
# DevDebug:  dev build with debug symbols and without optimizations (best for everyday development)
# DevOpt:    dev build with debug symbols and with optimizations
# Release:   release build without optimizations and without debug symbols (release build have for example assertions disabled)
# ReleaseOpt:release build with optimizations and without debug symbols
# Debug:     defaults to DevDebug


# std can use NDEBUG for internal assert purposes, so we should define it here
set(CMAKE_CXX_FLAGS_DEV        "-O0 -DBUILD_TYPE_DEV")
set(CMAKE_CXX_FLAGS_DEVDEBUG   "-O0 -DBUILD_TYPE_DEV -g -DBUILD_TYPE_DEV_DEBUG")
set(CMAKE_CXX_FLAGS_DEVOPT     "-O3 -DBUILD_TYPE_DEV")
set(CMAKE_CXX_FLAGS_RELEASE    "-O0 -DBUILD_TYPE_RELEASE -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASEOPT "-O3 -DBUILD_TYPE_RELEASE -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG      ${CMAKE_CXX_FLAGS_DEVDEBUG})


if(STRIP_SYMBOL_INFORMATION)
	# if not gcc/clang, this might fail:
	if (NOT (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
		message(FATAL_ERROR "Error: STRIP_SYMBOL_INFORMATION will likely fail (as is) compilers other then GCC and Clang. Fix or validate it first.")
	endif()
	if(APPLE)
		# ld64 still honours -s but prints "warning: -s is obsolete" on every link.
		# Instead, `-x -S` is the supported spelling and should produce the same result.
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-x,-S")
	else()
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s")
	endif()
endif()

if (ENABLE_LINK_TIME_OPTIMIZATION)
	include(CheckIPOSupported)
	check_ipo_supported()
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
	message("-- Link time optimization enabled")
endif()

if (SANITIZER)
	string(TOUPPER "${SANITIZER}" SANITIZER_UPPER)
	if (SANITIZER_UPPER STREQUAL "ASAN")
		message("-- AddressSanitizer enabled")
		add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
		add_link_options(-fsanitize=address)
	elseif (SANITIZER_UPPER STREQUAL "TSAN")
		message("-- ThreadSanitizer enabled")
		add_compile_options(-fsanitize=thread)
		add_link_options(-fsanitize=thread)
	elseif (SANITIZER_UPPER STREQUAL "UBSAN")
		message("-- UndefinedBehaviorSanitizer enabled")
		add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
		add_link_options(-fsanitize=undefined)
	else()
		message(FATAL_ERROR "Unknown sanitizer: ${SANITIZER}. Use ASAN, TSAN, or UBSAN.")
	endif()
endif()
