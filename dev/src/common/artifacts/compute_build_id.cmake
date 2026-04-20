# Computes a SHA256 fingerprint of the compiler source tree and writes it
# into build_id.hpp via configure_file. Intended to be invoked at build time
# (not configure time) through an add_custom_target running
#   cmake -DSRC_ROOT=... -DGEN_DIR=... -DTEMPLATE=... -P compute_build_id.cmake
#
# configure_file only rewrites the header when the content changes, so when
# sources are untouched ninja/ccache see the header as unchanged.

if(NOT SRC_ROOT)
	message(FATAL_ERROR "compute_build_id.cmake requires -DSRC_ROOT=<repo dev/ root>")
endif()
if(NOT GEN_DIR)
	message(FATAL_ERROR "compute_build_id.cmake requires -DGEN_DIR=<output dir>")
endif()
if(NOT TEMPLATE)
	message(FATAL_ERROR "compute_build_id.cmake requires -DTEMPLATE=<build_id.hpp.in>")
endif()

set(SRC_SUBDIRS
	"${SRC_ROOT}/src/base"
	"${SRC_ROOT}/src/common"
	"${SRC_ROOT}/src/compiler"
	"${SRC_ROOT}/src/vm"
)

set(ALL_FILES "")
foreach(SUBDIR ${SRC_SUBDIRS})
	if(NOT EXISTS "${SUBDIR}")
		continue()
	endif()
	file(GLOB_RECURSE SUBDIR_FILES
		"${SUBDIR}/*.hpp"
		"${SUBDIR}/*.cpp"
		"${SUBDIR}/*.in"
	)
	list(APPEND ALL_FILES ${SUBDIR_FILES})
endforeach()

# Exclude tests/, examples/, playground/ - these don't affect the compiler binary.
list(FILTER ALL_FILES EXCLUDE REGEX "/tests/")
list(FILTER ALL_FILES EXCLUDE REGEX "/examples/")
list(FILTER ALL_FILES EXCLUDE REGEX "/playground/")

list(SORT ALL_FILES)

set(ACCUMULATED "")
foreach(F ${ALL_FILES})
	file(SHA256 "${F}" FILE_HASH)
	# Include the relative path so file renames also change the id.
	file(RELATIVE_PATH REL_PATH "${SRC_ROOT}" "${F}")
	string(APPEND ACCUMULATED "${REL_PATH}:${FILE_HASH}\n")
endforeach()

string(SHA256 ARTIFACTS_BUILD_ID "${ACCUMULATED}")

file(MAKE_DIRECTORY "${GEN_DIR}/artifacts")
configure_file(
	"${TEMPLATE}"
	"${GEN_DIR}/artifacts/build_id.hpp"
	@ONLY
)
