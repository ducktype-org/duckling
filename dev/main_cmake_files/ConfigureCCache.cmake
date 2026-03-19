option(USE_CCACHE "Use ccache" OFF)

if(USE_CCACHE)
	message(NOTICE "Ccache might not work with generators other then make or ninja!")

	find_program(CCACHE "ccache" PATHS "scripts/downloads/" REQUIRED)

	if(CCACHE)
		message(STATUS "Ccache set to: ${CCACHE}")

		# As of 02.2024 there is no better way to set configuration other then listing it here:
		# @FUTURE It might change in the future
		set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE};cache_dir=${CMAKE_CURRENT_SOURCE_DIR}/.ccache/;max_size=1")
	else()
		message(ERROR "CCACHE not found.")
	endif(CCACHE)
endif(USE_CCACHE)
