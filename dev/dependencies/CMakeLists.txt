include(Asio.cmake)
include(Base64.cmake)
include(Crow.cmake)
include(Graphviz.cmake)
include(ICU.cmake)
include(NlohmannJson.cmake)
include(StdCxxExp.cmake)
include(TreeSitter.cmake)

# This macro inserts LLVM as a library to the project. It should be used in the top-level
# CMakeList.txt file.
# First it tries to find a downloaded version of LLVM located somewhere in `scripts/downloads`,
# and upon failing tries to use a system version of LLVM which is at least 19.1.x.
macro(insertLLVM)
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
endmacro()
