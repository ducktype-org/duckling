# This code should be executed in main CMakeLists.
# First it tries to find a preferred LLVM version (if provided), then falls back
# to the minimum supported LLVM version on the system.
set(DUCKLING_LLVM_FALLBACK_VERSION "19.1.0")

if (NOT DEFINED DUCKLING_LLVM_PREFERRED_VERSION)
	set(DUCKLING_LLVM_PREFERRED_VERSION "${DUCKLING_LLVM_FALLBACK_VERSION}")
endif ()

set(DUCKLING_LLVM_SEARCH_PATHS "scripts/downloads")
list(APPEND DUCKLING_LLVM_SEARCH_PATHS
	"/usr/lib/llvm-${DUCKLING_LLVM_PREFERRED_VERSION}/lib/cmake/llvm"
	"/usr/lib/llvm-${DUCKLING_LLVM_PREFERRED_VERSION}/cmake"
)
if (DEFINED DUCKLING_LLVM_PREFERRED_DIR)
	list(PREPEND DUCKLING_LLVM_SEARCH_PATHS "${DUCKLING_LLVM_PREFERRED_DIR}")
endif ()

find_package(LLVM ${DUCKLING_LLVM_PREFERRED_VERSION} CONFIG
	PATHS ${DUCKLING_LLVM_SEARCH_PATHS}
	NO_DEFAULT_PATH)
if (NOT LLVM_FOUND)
	# Some LLVMConfigVersion.cmake files require a full x.y.z match,
	# so retry without a version constraint and validate major version manually.
	find_package(LLVM CONFIG
		PATHS ${DUCKLING_LLVM_SEARCH_PATHS}
		NO_DEFAULT_PATH
		QUIET)
	if (LLVM_FOUND)
		string(REGEX MATCH "^([0-9]+)" LLVM_VERSION_MAJOR "${LLVM_PACKAGE_VERSION}")
		if (NOT LLVM_VERSION_MAJOR STREQUAL "${DUCKLING_LLVM_PREFERRED_VERSION}")
			set(LLVM_FOUND FALSE)
		endif ()
	endif ()
endif ()
if (NOT LLVM_FOUND)
	message(STATUS "Preferred LLVM not found, looking for local")
	find_package(LLVM ${DUCKLING_LLVM_PREFERRED_VERSION} CONFIG QUIET)
endif ()

if (NOT LLVM_FOUND)
	message(STATUS "Falling back to LLVM ${DUCKLING_LLVM_FALLBACK_VERSION}")
	find_package(LLVM ${DUCKLING_LLVM_FALLBACK_VERSION} REQUIRED CONFIG)
else()
	message(STATUS "Using custom LLVM")
endif ()
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
# @TODO: Mark all LLVM components as system with setAsSystemLibraries
