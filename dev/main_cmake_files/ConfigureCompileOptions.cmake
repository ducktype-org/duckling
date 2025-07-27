option(USE_MARCH_NATIVE "Use -march=native. This should be disabled for portable builds" OFF)

# disable compiler-specific extensions
set(CMAKE_CXX_EXTENSIONS OFF)
# require compiler to support C++ standard it is asked for
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	message("-- GNU compiler")

	string(CONCAT ADDITIONAL_GNU_FLAGS
		"-Werror=return-type "
		"-Werror=terminate "
		"-Werror=shadow=local "
		"-Werror=return-local-addr "
		"-Werror=free-nonheap-object "
		"-Werror=conversion "
		"-Werror=implicit-fallthrough "
		"-Wall -Wextra "
		"-pedantic "
		"-Wno-sign-compare "
		)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ADDITIONAL_GNU_FLAGS}")
	
	# Debug version uses O0.

elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	message("-- Clang compiler")

	# I didn't find a good -Werror=terminate alternative for Clang.
	# The "-Werror=shadow" is more strict than "-Werror=shadow=local".
	string(CONCAT ADDITIONAL_CLANG_FLAGS
		"-Werror=return-type "
		"-Werror=return-stack-address "
		"-Werror=free-nonheap-object "
		"-Werror=conversion "
		"-Werror=implicit-fallthrough "
		"-Wall -Wextra "
		"-pedantic "
		"-Wno-sign-compare "
		"-Wno-sign-conversion "
	)

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ADDITIONAL_CLANG_FLAGS}" )

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
# Release uses -O3 by default, but we want to use -O2 for now
set(CMAKE_CXX_FLAGS_DEV        "-O0 -DBUILD_TYPE_DEV")
set(CMAKE_CXX_FLAGS_DEVDEBUG   "-O0 -DBUILD_TYPE_DEV -g")
set(CMAKE_CXX_FLAGS_DEVOPT     "-O2 -DBUILD_TYPE_DEV")
set(CMAKE_CXX_FLAGS_RELEASE    "-O0 -DBUILD_TYPE_RELEASE -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASEOPT "-O2 -DBUILD_TYPE_RELEASE -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG      ${CMAKE_CXX_FLAGS_DEVDEBUG})


# Strip binaries from symbols in Release build
if(CMAKE_BUILD_TYPE MATCHES "^Release.*$")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -s")
endif()