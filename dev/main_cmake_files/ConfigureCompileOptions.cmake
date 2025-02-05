option(USE_MARCH_NATIVE "Use -march=native. This should be disabled for portable builds" OFF)

# disable compiler-specific extensions
set(CMAKE_CXX_EXTENSIONS OFF)
# require compiler to support C++ standard it is asked for
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	message("-- GNU compiler")
	# @GCC 15 (or later): remove -lstdc++exp, it is now needed for <stacktrace>
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-type -Werror=terminate -Werror=shadow=local -Werror=return-local-addr -Werror=free-nonheap-object -Wall -Wextra -Wno-sign-compare")

	# Debug version uses O0.
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")

elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	message("-- Clang compiler")

	# I didn't find a good -Werror=terminate alternative for Clang.
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-type -Werror=shadow-all -Werror=return-stack-address -Werror=free-nonheap-object -Wno-shadow-field-in-constructor -Wno-shadow-field -Wall -Wextra -Wno-sign-compare")

	# Debug version uses O0.
	# For some reason -Og does not work in clang
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
else()
	message(FATAL_ERROR "Error: UNKNOWN COMPILER")
endif()

if (USE_MARCH_NATIVE)
	add_compile_options(-march=native)
endif (USE_MARCH_NATIVE)

# Release version uses O2, not O3. It might change.
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
