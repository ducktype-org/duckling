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

if(APPLE)
	# LLVM's imported targets carry `<sdk>/usr/include` as a system include (it found
	# zlib/ffi there at build time). That path is redundant — the compiler already
	# searches the sysroot implicitly — and searching it early makes `<unicode/*.h>`
	# resolve to Apple's C-only ICU (which forces U_SHOW_CPLUSPLUS_API=0), shadowing
	# Homebrew ICU's C++ headers. Strip it from every imported target.
	get_property(DUCK_IMPORTED_TARGETS DIRECTORY PROPERTY IMPORTED_TARGETS)
	foreach(duck_imported_target ${DUCK_IMPORTED_TARGETS})
		foreach(duck_inc_prop INTERFACE_INCLUDE_DIRECTORIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES)
			get_target_property(duck_incs ${duck_imported_target} ${duck_inc_prop})
			if(duck_incs)
				list(FILTER duck_incs EXCLUDE REGEX "/usr/include$")
				set_target_properties(
					${duck_imported_target} PROPERTIES ${duck_inc_prop} "${duck_incs}"
				)
			endif()
		endforeach()
	endforeach()

	# Same reason: keep the redundant sysroot include out of LLVM_INCLUDE_DIRS.
	if(LLVM_INCLUDE_DIRS)
		list(FILTER LLVM_INCLUDE_DIRS EXCLUDE REGEX "/usr/include$")
	endif()
endif()
