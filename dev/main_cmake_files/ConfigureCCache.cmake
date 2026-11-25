option(USE_CCACHE "Use ccache" OFF)

if(USE_CCACHE)
	message(NOTICE "Ccache might not work with generators other then make or ninja!")

	set(SHIPPED_BINARIES_FILE "${CMAKE_CURRENT_SOURCE_DIR}/scripts/shipped_binaries.json")
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SHIPPED_BINARIES_FILE}")
	file(READ "${SHIPPED_BINARIES_FILE}" SHIPPED_BINARIES)
	string(JSON CCACHE_SHIPPED_VERSION GET "${SHIPPED_BINARIES}" ccache)

	set(CCACHE_FIX_HINT "Run `./toolbox.py download-binaries -s ccache -f` to get ccache ${CCACHE_SHIPPED_VERSION}.")

	# NO_CACHE so a ccache downloaded after the first configure is picked up.
	find_program(CCACHE "ccache" HINTS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/downloads/" NO_CACHE)
	if(NOT CCACHE)
		message(FATAL_ERROR "ccache not found. ${CCACHE_FIX_HINT}")
	endif()

	execute_process(
		COMMAND "${CCACHE}" --version
		OUTPUT_VARIABLE CCACHE_VERSION_OUTPUT
		ERROR_QUIET
	)
	string(REGEX MATCH "ccache version ([0-9.]+)" _ "${CCACHE_VERSION_OUTPUT}")
	set(CCACHE_FOUND_VERSION "${CMAKE_MATCH_1}")

	if(NOT CCACHE_FOUND_VERSION VERSION_EQUAL CCACHE_SHIPPED_VERSION)
		message(FATAL_ERROR
			"ccache at ${CCACHE} is version '${CCACHE_FOUND_VERSION}', but Duckling requires "
			"${CCACHE_SHIPPED_VERSION}. ${CCACHE_FIX_HINT}"
		)
	endif()

	message(STATUS "Ccache set to: ${CCACHE} (${CCACHE_FOUND_VERSION})")

	# As of 02.2024 there is no better way to set configuration other then listing it here:
	# @FUTURE It might change in the future
	set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE};cache_dir=${CMAKE_CURRENT_SOURCE_DIR}/.ccache/;max_size=1")
endif(USE_CCACHE)
