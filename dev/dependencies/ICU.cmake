include(FetchContent)

# released 2026-01-09
set(ICU_VERSION_MAJOR "78")
set(ICU_VERSION_MINOR "2")
set(ICU_VERSION_REQUIRED "${ICU_VERSION_MAJOR}.${ICU_VERSION_MINOR}")
set(ICU_RELEASE_PREFIX "https://github.com/unicode-org/icu/releases/download/release-${ICU_VERSION_REQUIRED}/icu4c-${ICU_VERSION_REQUIRED}-")
set(ICU_RELEASE "${ICU_RELEASE_PREFIX}sources.tgz")
set(ICU_CONTROL "SHA512=92feddfe81c57336f386c7cbc9f6d976bf349db148a77a247c4559676f51116115c8c52c4d907feb50933f72ab75fd8e48be092bf9c8ca33a3e8fabc9372a5d6")

# Used in github actions
# The ubuntu release cannot simply be set to the latest and shiniest version,
# check the available one on the ICU github release page.
set(ICU_UBUNTU_VERSION "22.04")
set(ICU_UBUNTU_RELEASE "${ICU_RELEASE_PREFIX}Ubuntu${ICU_UBUNTU_VERSION}-x64.tgz")
set(ICU_UBUNTU_CONTROL "SHA512=58c74a109fb32c22315d78f7a4c913deaea6b0a55553759b9efb817f4b18587862cb756fb165d352ccfe17591e5354b90aab25f6d05de56172e38a129ee73c5e")

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
	elseif(DOWNLOAD_UBUNTU_ICU_BUILD STREQUAL "ON")
		# Be careful this is mostly for github actions
        message("-- Downloading ubuntu icu build for ubuntu ${ICU_UBUNTU_VERSION}")

        set(ICU_RELEASE "${ICU_UBUNTU_RELEASE}")
        set(ICU_CONTROL "${ICU_UBUNTU_CONTROL}")

		FetchContent_Declare(
			ubuntu-icu
			URL ${ICU_RELEASE}
			URL_HASH ${ICU_CONTROL}
			SYSTEM
		)

		FetchContent_MakeAvailable(ubuntu-icu)

		set(ICU_PREFIX ${PROJECT_BINARY_DIR}/_deps/ubuntu-icu-src/usr/local)
		set(ICU_INCLUDE_DIRS ${ICU_PREFIX}/include)

        # Even though there are symlinks for the exact version, it ends up not being able to find it.
        # This has to use only the major.
        set(ICU_DATA_LIBRARY "${ICU_PREFIX}/lib/libicudata.so.${ICU_VERSION_MAJOR}")
		set(ICU_I18N_LIBRARY "${ICU_PREFIX}/lib/libicui18n.so.${ICU_VERSION_MAJOR}")
		set(ICU_UC_LIBRARY "${ICU_PREFIX}/lib/libicuuc.so.${ICU_VERSION_MAJOR}")
		set(ICU_IO_LIBRARY "${ICU_PREFIX}/lib/libicuio.so.${ICU_VERSION_MAJOR}")

        add_library(icudata IMPORTED SHARED GLOBAL)
        set_target_properties(icudata PROPERTIES IMPORTED_LOCATION ${ICU_DATA_LIBRARY})
        target_include_directories(icudata INTERFACE ${ICU_INCLUDE_DIRS})

        add_library(icui18n IMPORTED SHARED GLOBAL)
        set_target_properties(icui18n PROPERTIES
            IMPORTED_LOCATION ${ICU_I18N_LIBRARY}
            IMPORTED_LINK_DEPENDENT_LIBRARIES "icudata"   # icui18n.so needs icudata.so
        )
        target_include_directories(icui18n INTERFACE ${ICU_INCLUDE_DIRS})

        add_library(icuuc IMPORTED SHARED GLOBAL)
        set_target_properties(icuuc PROPERTIES
            IMPORTED_LOCATION ${ICU_UC_LIBRARY}
            IMPORTED_LINK_DEPENDENT_LIBRARIES "icudata;icui18n"  # icuuc.so needs both
        )
        target_include_directories(icuuc INTERFACE ${ICU_INCLUDE_DIRS})

        add_library(icuio IMPORTED SHARED GLOBAL)
        set_target_properties(icuio PROPERTIES
            IMPORTED_LOCATION ${ICU_IO_LIBRARY}
            IMPORTED_LINK_DEPENDENT_LIBRARIES "icuuc;icudata;icui18n"
        )
        target_include_directories(icuio INTERFACE ${ICU_INCLUDE_DIRS})

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

	add_library(icui18n IMPORTED SHARED GLOBAL)
	set_target_properties(icui18n PROPERTIES IMPORTED_LOCATION ${ICU_I18N_LIBRARY})
	target_include_directories(icui18n INTERFACE ${ICU_INCLUDE_DIRS})

	add_library(icuuc IMPORTED SHARED GLOBAL)
	set_target_properties(icuuc PROPERTIES IMPORTED_LOCATION ${ICU_UC_LIBRARY})
	target_include_directories(icuuc INTERFACE ${ICU_INCLUDE_DIRS})

	add_library(icuio IMPORTED SHARED GLOBAL)
	set_target_properties(icuio PROPERTIES IMPORTED_LOCATION ${ICU_IO_LIBRARY})
	target_include_directories(icuio INTERFACE ${ICU_INCLUDE_DIRS})

endif()

add_library(unicode INTERFACE)
target_link_libraries(unicode INTERFACE icui18n icuuc icuio icudata)
set(ICU_LIBRARIES icui18n icuuc icuio icudata)

message("-- ICU version: ${ICU_VERSION}")
message("-- ICU include dirs: ${ICU_INCLUDE_DIRS}")
message("-- ICU libraries: ${ICU_LIBRARIES}")


if (ICU_IS_EXTERNAL)
	file(MAKE_DIRECTORY ${ICU_INCLUDE_DIRS})
endif()
