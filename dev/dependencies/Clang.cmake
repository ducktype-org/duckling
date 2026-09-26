# Must be included after LLVM.cmake. ClangConfig re-finds LLVM with an EXACT version requirement,
# so the search is anchored to the LLVM that was already found rather than letting it pick another
# install. LLVM_DIR can be a symlink (`/usr/lib/llvm-19/cmake` on Debian), so the real location is
# derived from LLVM_CMAKE_DIR, which LLVMConfig sets to `<prefix>/lib/cmake/llvm`.
get_filename_component(DUCK_LLVM_CMAKE_PARENT "${LLVM_CMAKE_DIR}" DIRECTORY)

find_package(Clang ${LLVM_PACKAGE_VERSION} EXACT CONFIG QUIET
	PATHS
		"${DUCK_LLVM_CMAKE_PARENT}/clang"
		"${LLVM_INSTALL_PREFIX}/lib/cmake/clang"
	NO_DEFAULT_PATH
)

if (NOT Clang_FOUND)
	message(STATUS "Clang not found next to LLVM, looking for local")
	find_package(Clang ${LLVM_PACKAGE_VERSION} EXACT CONFIG QUIET)
endif ()

# libclang is needed only by duck_c_import, so a missing one disables that tool instead of
# blocking every other build.
if (Clang_FOUND AND TARGET libclang)
	set(DUCK_HAS_LIBCLANG ON)
	message(STATUS "Using ClangConfig.cmake in: ${Clang_DIR}")
	# libclang resolves its builtin headers (stddef.h, stdarg.h, ...) relative to its own
	# location, which does not work when it is loaded as a library, so the path is baked in.
	set(DUCK_CLANG_RESOURCE_DIR "${LLVM_LIBRARY_DIR}/clang/${LLVM_VERSION_MAJOR}")
else ()
	set(DUCK_HAS_LIBCLANG OFF)
	message(STATUS
		"libclang not found - duck_c_import will not be built. "
		"On Debian/Ubuntu install `libclang-19-dev`."
	)
endif ()
