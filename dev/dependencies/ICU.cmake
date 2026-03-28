include(FetchContent)

set (BUILD_STATIC_ICU true)
set (FindOrBuildICU_URL https://github.com/unicode-org/icu/releases/download/release-74-1/icu4c-74_1-src.tgz)
# set (FindOrBuildICU_URL https://github.com/unicode-org/icu/archive/refs/tags/release-74-1.tar.gz)



set(ICU_VERSION_REQUIRED "74.1")
set(ICU_RELEASE "https://github.com/unicode-org/icu/releases/download/release-74-1/icu4c-74_1-src.tgz")
set(ICU_CONTROL "SHA512=32c28270aa5d94c58d2b1ef46d4ab73149b5eaa2e0621d4a4c11597b71d146812f5e66db95f044e8aaa11b94e99edd4a48ab1aa8efbe3d72a73870cd56b564c2")

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
			# limit the number of cores to 4 on travis
			if (CORES GREATER 4)
				if ($ENV{TRAVIS})
					set(CORES 4)
				endif()
			endif()
			set(ICU_MAKE_EXTRA_FLAGS "-j${CORES}")
		endif()

		set(ICU_EP_PREFIX ${PROJECT_BINARY_DIR}/_deps/icu-${FindOrBuildICU_VERSION}-build)

		set(ICU_EP_LIBICUDATA ${ICU_EP_PREFIX}/lib/libicudata.a)
		set(ICU_EP_LIBICUI18N ${ICU_EP_PREFIX}/lib/libicui18n.a)
		set(ICU_EP_LIBICUUC ${ICU_EP_PREFIX}/lib/libicuuc.a)
		set(ICU_EP_LIBICUIO ${ICU_EP_PREFIX}/lib/libicuio.a)
		set(ICU_EP_PATCH_COMMAND "")

		ExternalProject_Add(ExternalICU
			PREFIX
				${ICU_EP_PREFIX}
			DOWNLOAD_DIR
				${PROJECT_BINARY_DIR}/_deps/icu-${FindOrBuildICU_VERSION}-src
			URL
				${FindOrBuildICU_URL}
			URL_HASH
				${FindOrBuildICU_URL_HASH}
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

		add_library(icudata IMPORTED STATIC GLOBAL)
		set_target_properties(icudata PROPERTIES IMPORTED_LOCATION
			${ICU_EP_LIBICUDATA})
		add_dependencies(icudata ExternalICU)
		target_include_directories(icudata INTERFACE ${ICU_INCLUDE_DIRS})

		add_library(icui18n IMPORTED STATIC GLOBAL)
		set_target_properties(icui18n PROPERTIES IMPORTED_LOCATION
			${ICU_EP_LIBICUI18N})
		add_dependencies(icui18n ExternalICU)
		target_include_directories(icui18n INTERFACE ${ICU_INCLUDE_DIRS})

		add_library(icuuc IMPORTED STATIC GLOBAL)
		set_target_properties(icuuc PROPERTIES IMPORTED_LOCATION
			${ICU_EP_LIBICUUC})
		add_dependencies(icuuc ExternalICU)
		target_include_directories(icuuc INTERFACE ${ICU_INCLUDE_DIRS})

		add_library(icuio IMPORTED STATIC GLOBAL)
		set_target_properties(icuio PROPERTIES IMPORTED_LOCATION
			${ICU_EP_LIBICUIO})
		add_dependencies(icuio ExternalICU)
		target_include_directories(icuio INTERFACE ${ICU_INCLUDE_DIRS})
		set(ICU_IS_EXTERNAL TRUE PARENT_SCOPE)
	else()
		message(FATAL_ERROR "-- ICU building not supported for this platform")
	endif()
else()
	message("-- Using local ICU")

	add_library(icudata IMPORTED SHARED GLOBAL)
	set_target_properties(icudata PROPERTIES IMPORTED_LOCATION ${ICU_DATA_LIBRARY})
	target_include_directories(icudata INTERFACE ${ICU_INCLUDE_DIRS})

	add_library(icu18n IMPORTED SHARED GLOBAL)
	set_target_properties(icu18n PROPERTIES IMPORTED_LOCATION ${ICU_I18N_LIBRARY})
	target_include_directories(icu18n INTERFACE ${ICU_INCLUDE_DIRS})

	add_library(icuuc IMPORTED SHARED GLOBAL)
	set_target_properties(icuuc PROPERTIES IMPORTED_LOCATION ${ICU_UC_LIBRARY})
	target_include_directories(icuuc INTERFACE ${ICU_INCLUDE_DIRS})

	add_library(icuio IMPORTED SHARED GLOBAL)
	set_target_properties(icuio PROPERTIES IMPORTED_LOCATION ${ICU_IO_LIBRARY})
	target_include_directories(icuio INTERFACE ${ICU_INCLUDE_DIRS})

endif()

add_library(unicode INTERFACE)
# target_link_libraries(unicode INTERFACE ICU::i18n ICU::uc ICU::io ICU::data)
target_link_libraries(unicode INTERFACE icui18n icuuc icuio icudata)
set(ICU_LIBRARIES icui18n icuuc icuio icudata)

message("-- ICU version: ${ICU_VERSION}")
message("-- ICU include dirs: ${ICU_INCLUDE_DIRS}")
message("-- ICU libraries: ${ICU_LIBRARIES}")


if (ICU_IS_EXTERNAL)
	file(MAKE_DIRECTORY ${ICU_INCLUDE_DIRS})
endif()
