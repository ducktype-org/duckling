# This code should be executed in main CMakeLists.
# First it tries to find a downloaded version of LLVM located somewhere in `scripts/downloads`,
# and upon failing tries to use a system version of LLVM which is at least 19.1.x.
find_package(LLVM 19.1.0 CONFIG PATHS scripts/downloads NO_DEFAULT_PATH)
if (NOT ${LLVM_FOUND} EQUAL 1)
	message(STATUS "Custom LLVM not found, looking for local")
	find_package(LLVM 19.1.0 REQUIRED CONFIG)
else()
	message(STATUS "Using custom LLVM")
endif ()
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
# @TODO: Mark all LLVM components as system with setAsSystemLibraries
