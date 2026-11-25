option(USE_CCACHE "Use ccache" OFF)

if(USE_CCACHE)
	message(NOTICE "Ccache might not work with generators other then make or ninja!")

	# 4.10 is the first version that caches -fprofile-update (coverage) compiles.
	set(CCACHE_MIN_VERSION "4.10")
	set(CCACHE_FIX_HINT "Run `./toolbox.py download-binaries -s ccache` to get the latest ccache.")

	find_program(CCACHE_PROGRAM "ccache" HINTS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/downloads/" NO_CACHE)
	if(NOT CCACHE_PROGRAM)
		message(FATAL_ERROR "ccache not found. ${CCACHE_FIX_HINT}")
	endif()

	execute_process(
		COMMAND "${CCACHE_PROGRAM}" --version
		OUTPUT_VARIABLE CCACHE_VERSION_OUTPUT
		ERROR_QUIET
	)
	string(REGEX MATCH "ccache version ([0-9.]+)" _ "${CCACHE_VERSION_OUTPUT}")
	set(CCACHE_FOUND_VERSION "${CMAKE_MATCH_1}")

	if(NOT CCACHE_FOUND_VERSION OR CCACHE_FOUND_VERSION VERSION_LESS CCACHE_MIN_VERSION)
		message(FATAL_ERROR
			"ccache at ${CCACHE_PROGRAM} is version '${CCACHE_FOUND_VERSION}', but Duckling requires "
			"${CCACHE_MIN_VERSION} or newer. ${CCACHE_FIX_HINT}"
		)
	endif()

	message(STATUS "Ccache set to: ${CCACHE_PROGRAM} (${CCACHE_FOUND_VERSION})")

	# As of 02.2024 there is no better way to set configuration other then listing it here:
	# @FUTURE It might change in the future
	set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM};cache_dir=${CMAKE_CURRENT_SOURCE_DIR}/.ccache/;max_size=1")
endif(USE_CCACHE)
