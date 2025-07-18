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

# Debug usees -O0, DevRelease and Release use -O2.
set(CMAKE_CXX_FLAGS_DEBUG      "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -DBUILD_TYPE_DEBUG")
set(CMAKE_CXX_FLAGS_DEVRELEASE "${CMAKE_CXX_FLAGS_DEVRELEASE} -O2 -DBUILD_TYPE_DEV_RELEASE")
set(CMAKE_CXX_FLAGS_RELEASE    "-O2 -DBUILD_TYPE_RELEASE")
