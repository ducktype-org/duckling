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

# Some LLVM installs ship only libLLVM.so, without the per-component libraries (LLVMCore, ...),
# so there is nothing to link per component. Configure those with -DLLVM_TYPE=SINGLE_LIB.
set(LLVM_TYPE "COMPONENTS" CACHE STRING "How to link LLVM: per-component libraries or the single libLLVM")
set_property(CACHE LLVM_TYPE PROPERTY STRINGS "COMPONENTS" "SINGLE_LIB")

# Puts given LLVM libraries into out_var (wrapper over llvm_map_components_to_libnames
# OR, when SINGLE_LIB is set, puts libLLVM into out_var.
# Note: we could probably let LLVM pick the libraries itself (llvm_config() or `llvm-config --libs`)
# and remove LLVM_TYPE.
function(duck_llvm_libs out_var)
	if(LLVM_TYPE STREQUAL "SINGLE_LIB")
		if(NOT TARGET LLVM)
			message(FATAL_ERROR
				"LLVM_TYPE is SINGLE_LIB, but LLVM ${LLVM_PACKAGE_VERSION} (${LLVM_DIR}) has no libLLVM "
				"target. Configure with -DLLVM_TYPE=COMPONENTS.")
		endif()
		set(${out_var} LLVM PARENT_SCOPE)
	elseif(LLVM_TYPE STREQUAL "COMPONENTS")
		llvm_map_components_to_libnames(libs ${ARGN})
		set(${out_var} ${libs} PARENT_SCOPE)
	else()
		message(FATAL_ERROR "Unknown LLVM_TYPE '${LLVM_TYPE}', expected COMPONENTS or SINGLE_LIB")
	endif()
endfunction()

# An LLVM built with zstd support (LLVM_ENABLE_ZSTD, e.g. Homebrew's or Ubuntu's) makes LLVMSupport
# link the shared zstd library. With LINK_STATIC_ZSTD, the static zstd library is linked instead.
#  macOS has no system zstd, so portable macOS binaries need it.
option(LINK_STATIC_ZSTD "Link the zstd library that LLVM depends on statically (portable binaries on macOS)." OFF)
if(LINK_STATIC_ZSTD)
	if(NOT LLVM_ENABLE_ZSTD)
		message(STATUS "LINK_STATIC_ZSTD: LLVM ${LLVM_PACKAGE_VERSION} is built without zstd, nothing to link")
	elseif(LLVM_TYPE STREQUAL "SINGLE_LIB")
		# libLLVM.so has zstd linked in already, so patching LLVMSupport below would change nothing
		# (and an LLVM that ships only libLLVM has no LLVMSupport target at all).
		message(FATAL_ERROR
			"LINK_STATIC_ZSTD does not work with LLVM_TYPE=SINGLE_LIB: libLLVM already links zstd itself. "
			"Configure with -DLLVM_TYPE=COMPONENTS or without LINK_STATIC_ZSTD.")
	else()
		# LLVMConfig.cmake ran find_package(zstd), which defines zstd::libzstd_static when a static
		# library (libzstd.a) is installed next to the shared one.
		if(NOT TARGET zstd::libzstd_static)
			message(FATAL_ERROR
				"LINK_STATIC_ZSTD is ON, but no static zstd library was found (zstd_STATIC_LIBRARY: "
				"'${zstd_STATIC_LIBRARY}'). Install one (libzstd.a) or configure without LINK_STATIC_ZSTD.")
		endif()
		get_target_property(duck_llvm_support_libs LLVMSupport INTERFACE_LINK_LIBRARIES)
		if(NOT duck_llvm_support_libs MATCHES "zstd::libzstd_(shared|static)")
			message(FATAL_ERROR
				"LINK_STATIC_ZSTD: LLVMSupport links zstd in an unexpected way: '${duck_llvm_support_libs}'")
		endif()
		# Also inside generator expressions such as $<LINK_ONLY:zstd::libzstd_shared>.
		list(TRANSFORM duck_llvm_support_libs REPLACE "zstd::libzstd_shared" "zstd::libzstd_static")
		set_target_properties(LLVMSupport PROPERTIES INTERFACE_LINK_LIBRARIES "${duck_llvm_support_libs}")
		get_target_property(duck_zstd_static zstd::libzstd_static IMPORTED_LOCATION)
		message(STATUS "Linking zstd statically: ${duck_zstd_static}")
	endif()
endif()
