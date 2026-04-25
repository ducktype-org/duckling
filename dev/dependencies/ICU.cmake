include(FetchContent)

set(BUILD_STATIC_ICU "OFF" CACHE STRING "Whether to force building and linking against a custom-built static version of ICU.")

# released 2026-01-09
set(ICU_VERSION_MAJOR "74")
set(ICU_VERSION_MINOR "2")
set(ICU_VERSION_REQUIRED "${ICU_VERSION_MAJOR}.${ICU_VERSION_MINOR}")
set(ICU_RELEASE "https://github.com/unicode-org/icu/releases/download/release-${ICU_VERSION_MAJOR}-${ICU_VERSION_MINOR}/icu4c-${ICU_VERSION_MAJOR}_${ICU_VERSION_MINOR}-src.tgz")
set(ICU_CONTROL "SHA512=e6c7876c0f3d756f3a6969cad9a8909e535eeaac352f3a721338b9cbd56864bf7414469d29ec843462997815d2ca9d0dab06d38c37cdd4d8feb28ad04d8781b0")

# Based on: https://github.com/meta-toolkit/meta-cmake/blob/master/FindOrBuildICU.cmake
# Windows building is removed because it's very version-dependent

include(ExternalProject)

# Searches the system using find_package for an ICU version that is greater
# or equal to the minimum version (ICU_VERSION_REQUIRED).
# If find_package does not find a suitable version, ICU is added
# as an external project to be downloaded form the specified URL and
# validated with the specified URL_HASH.

# ICU_LIBRARIES and ICU_INCLUDE_DIRS variables are set
# for use by targets that wish to use ICU headers or ICU library functions.
message("-- Searching for ICU ${ICU_VERSION_REQUIRED}")

if(NOT BUILD_STATIC_ICU)
	find_package(ICU ${ICU_VERSION_REQUIRED} COMPONENTS data i18n uc io)
endif()

if(BUILD_STATIC_ICU OR NOT ICU_VERSION OR NOT ICU_VERSION VERSION_GREATER_EQUAL "${ICU_VERSION_REQUIRED}")
	# for some reason, ICU_FOUND seems to always be set...
	if(BUILD_STATIC_ICU)
		message("-- Building a static ICU was forced")
	else()
		if(NOT ICU_VERSION)
			message("-- ICU not found; attempting to build it...")
		else()
			message("-- ICU version found is ${ICU_VERSION}, expected ${ICU_VERSION_REQUIRED}; attempting to build ICU from scratch...")
		endif()
	endif()

	if(WIN32)
		# not going to attempt to build ICU if we're on Windows for now
		# probably could, but it's more trouble than it's worth I think
		message("-- ICU building not supported on Windows.")
		message(FATAL_ERROR "   -- Please download the latest ICU binaries from http://site.icu-project.org/download")

	elseif(UNIX)
		set(ICU_CFLAGS "-w")
		set(ICU_CXXFLAGS "-w")

		# determine a reasonable number of threads to build ICU with
		include(ProcessorCount)
		ProcessorCount(CORES)
		if (NOT CORES EQUAL 0)
			set(ICU_MAKE_EXTRA_FLAGS "-j${CORES}")
		endif()

		set(ICU_EP_PREFIX ${PROJECT_BINARY_DIR}/_deps/icu-${ICU_VERSION_REQUIRED}-build)

		set(ICU_EP_LIBICUDATA ${ICU_EP_PREFIX}/lib/libicudata.a)
		set(ICU_EP_LIBICUI18N ${ICU_EP_PREFIX}/lib/libicui18n.a)
		set(ICU_EP_LIBICUUC ${ICU_EP_PREFIX}/lib/libicuuc.a)
		set(ICU_EP_LIBICUIO ${ICU_EP_PREFIX}/lib/libicuio.a)
		set(ICU_EP_PATCH_COMMAND "")

		ExternalProject_Add(ExternalICU
			PREFIX
				${ICU_EP_PREFIX}
			DOWNLOAD_DIR
				${PROJECT_BINARY_DIR}/_deps/icu-${ICU_VERSION_REQUIRED}-src
			URL
				${ICU_RELEASE}
			URL_HASH
				${ICU_CONTROL}
			PATCH_COMMAND
				${ICU_EP_PATCH_COMMAND}
			CONFIGURE_COMMAND
				${CMAKE_COMMAND} -E env
				CC=${CMAKE_C_COMPILER}
				CXX=${CMAKE_CXX_COMPILER}
				CFLAGS=${ICU_CFLAGS}
				CXXFLAGS=${ICU_CXXFLAGS}
				sh ${ICU_EP_PREFIX}/src/ExternalICU/source/configure
				--disable-shared --enable-static --disable-dyload --disable-extras
				--disable-tests --disable-samples --quiet
				--prefix=<INSTALL_DIR>
			BUILD_COMMAND
				make ${ICU_MAKE_EXTRA_FLAGS} > /dev/null
			INSTALL_COMMAND
				make install -s > /dev/null
			BUILD_BYPRODUCTS
				${ICU_EP_LIBICUDATA};${ICU_EP_LIBICUI18N};${ICU_EP_LIBICUUC};${ICU_EP_LIBICUIO}
		)

		set(ICU_INCLUDE_DIRS ${ICU_EP_PREFIX}/include)

		file(MAKE_DIRECTORY ${ICU_INCLUDE_DIRS})

		add_library(ICU::data IMPORTED STATIC GLOBAL)
		set_target_properties(ICU::data PROPERTIES IMPORTED_LOCATION
			${ICU_EP_LIBICUDATA})
		add_dependencies(ICU::data ExternalICU)
		target_include_directories(ICU::data INTERFACE ${ICU_INCLUDE_DIRS})

		add_library(ICU::i18n IMPORTED STATIC GLOBAL)
		set_target_properties(ICU::i18n PROPERTIES IMPORTED_LOCATION
			${ICU_EP_LIBICUI18N})
		add_dependencies(ICU::i18n ExternalICU)
		target_include_directories(ICU::i18n INTERFACE ${ICU_INCLUDE_DIRS})

		add_library(ICU::uc IMPORTED STATIC GLOBAL)
		set_target_properties(ICU::uc PROPERTIES IMPORTED_LOCATION
			${ICU_EP_LIBICUUC})
		add_dependencies(ICU::uc ExternalICU)
		target_include_directories(ICU::uc INTERFACE ${ICU_INCLUDE_DIRS})

		add_library(ICU::io IMPORTED STATIC GLOBAL)
		set_target_properties(ICU::io PROPERTIES IMPORTED_LOCATION
			${ICU_EP_LIBICUIO})
		add_dependencies(ICU::io ExternalICU)
		target_include_directories(ICU::io INTERFACE ${ICU_INCLUDE_DIRS})
		set(ICU_IS_EXTERNAL TRUE PARENT_SCOPE)
	else()
		message(FATAL_ERROR "-- ICU building not supported for this platform")
	endif()
else()
	message("-- Using local ICU")
	# Note that in this branch the ICU::i18n ICU::uc ICU::data ICU::io
	# targets are provided by find_package(ICU).

endif()

add_library(unicode INTERFACE)
target_link_libraries(unicode INTERFACE ICU::i18n ICU::uc ICU::io ICU::data)

message("-- ICU version: ${ICU_VERSION}")
message("-- ICU include dirs: ${ICU_INCLUDE_DIRS}")
message("-- ICU libraries: ${ICU_LIBRARIES}")


if (ICU_IS_EXTERNAL)
	file(MAKE_DIRECTORY ${ICU_INCLUDE_DIRS})
endif()
