if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	# @TODO: decide of std++20 vs gnu++20
	message("-- GNU compiler")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-type -Werror=terminate -Werror=shadow=local -Werror=return-local-addr -Werror=free-nonheap-object -Wall -Wextra -Wno-sign-compare")

	# Debug version uses Og and prints all logs. 
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DPRINT_LOG -g -Og")

elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	message("-- Clang compiler")

	# I didn't find a good -Werror=terminate alternative for Clang.
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-type -Werror=shadow-all -Werror=return-stack-address -Werror=free-nonheap-object -Wno-shadow-field-in-constructor -Wno-shadow-field -Wall -Wextra -Wno-sign-compare")

	# Debug version uses O0 and prints all logs. 
	# For some reason -Og does not work in clang
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DPRINT_LOG -g -O0")
else()
	message(FATAL_ERROR "Error: UNKNOWN COMPILER")
endif()

# "-O2" here is needed so standard "cmake .." is compiled with O2.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

# Release version uses O2, not O3. It might change.
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
