# libclang (the stable C API) is needed only by duck_c_import, so a missing one disables that tool
# instead of failing the configure. It is looked up next to the LLVM already found, rather than
# through ClangConfig.cmake, which on Debian-style installs references files from packages that
# may not be installed.
find_library(DUCK_LIBCLANG_LIBRARY
	NAMES clang libclang
	HINTS "${LLVM_LIBRARY_DIR}"
	NO_DEFAULT_PATH
)
find_path(DUCK_LIBCLANG_INCLUDE_DIR
	NAMES clang-c/Index.h
	HINTS ${LLVM_INCLUDE_DIRS}
	NO_DEFAULT_PATH
)

if (DUCK_LIBCLANG_LIBRARY AND DUCK_LIBCLANG_INCLUDE_DIR)
	set(DUCK_HAS_LIBCLANG ON)
	message(STATUS "Using libclang: ${DUCK_LIBCLANG_LIBRARY}")

	add_library(duck_libclang INTERFACE IMPORTED GLOBAL)
	target_link_libraries(duck_libclang INTERFACE "${DUCK_LIBCLANG_LIBRARY}")
	target_include_directories(duck_libclang SYSTEM INTERFACE "${DUCK_LIBCLANG_INCLUDE_DIR}")

	# libclang looks for its builtin headers (stddef.h, stdbool.h, ...) relative to the executable
	# that loads it, which is wrong for a library user, so the resource directory is baked in.
	set(DUCK_CLANG_RESOURCE_DIR "${LLVM_LIBRARY_DIR}/clang/${LLVM_VERSION_MAJOR}")
else ()
	set(DUCK_HAS_LIBCLANG OFF)
	message(STATUS
		"libclang not found next to LLVM - duck_c_import will not be built. "
		"On Debian/Ubuntu install `libclang-19-dev`."
	)
endif ()
