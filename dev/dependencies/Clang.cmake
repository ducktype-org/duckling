# Must be included after LLVM.cmake: ClangConfig re-finds LLVM with an EXACT version
# requirement, so anchor the search to the prefix LLVM was already found in rather than
# letting it pick up a different install.
get_filename_component(DUCK_LLVM_CMAKE_PARENT "${LLVM_DIR}" DIRECTORY)
find_package(Clang CONFIG QUIET PATHS "${DUCK_LLVM_CMAKE_PARENT}/clang" NO_DEFAULT_PATH)
if (NOT Clang_FOUND)
	message(STATUS "Clang not found next to LLVM, looking for local")
	find_package(Clang REQUIRED CONFIG)
endif ()

if (NOT TARGET libclang)
	message(FATAL_ERROR
		"Clang was found in ${Clang_DIR} but it does not provide the `libclang` target. "
		"On Debian/Ubuntu install `libclang-19-dev`."
	)
endif ()

message(STATUS "Using ClangConfig.cmake in: ${Clang_DIR}")
