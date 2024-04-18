# Based on: https://github.com/meta-toolkit/meta-cmake/blob/master/FindOrBuildICU.cmake
# With Windows building removed because it's very dependent on version
# @TODO: It might be possible to add include directory automaticly from target

include(ExternalProject)

set(BUILD_ICU_DIR ${CMAKE_CURRENT_LIST_DIR})

# Searches the system using find_package for an ICU version that is greater
# or equal to the minimum version specified via the VERSION argument to the
# function. If find_package does not find a suitable version, ICU is added
# as an external project to be downloaded form the specified URL and
# validated with the specified URL_HASH.

# The function sets the variables ICU_LIBRARIES and ICU_INCLUDE_DIRS
# for use by targets that wish to use ICU headers or ICU library functions.
#
# This function requires at least CMake version 3.2.0 for the
# BUILD_BYPRODUCTS argument to ExternalProject_Add
function(FindOrBuildICU)
  set(oneValueArgs VERSION URL URL_HASH)
  cmake_parse_arguments(FindOrBuildICU "" "${oneValueArgs}" "" ${ARGN})

  if (NOT FindOrBuildICU_VERSION)
    message(FATAL_ERROR "You must provide a minimum version")
  endif()

  if (NOT FindOrBuildICU_URL)
    message(FATAL_ERROR "You must provide a download url to the ICU sources")
  endif()

  message("-- Searching for ICU ${FindOrBuildICU_VERSION}")

  if (NOT BUILD_STATIC_ICU)
    find_package(ICU ${FindOrBuildICU_VERSION} COMPONENTS data i18n uc io)
  endif()

  if (BUILD_STATIC_ICU OR NOT ICU_VERSION OR NOT ICU_VERSION VERSION_GREATER_EQUAL "${FindOrBuildICU_VERSION}")
    # for some reason, ICU_FOUND seems to always be set...
    if (BUILD_STATIC_ICU)
      message("-- Building a static ICU was forced")
    else()
      if (NOT ICU_VERSION)
        message("-- ICU not found; attempting to build it...")
      else()
        message("-- ICU version found is ${ICU_VERSION}, expected ${FindOrBuildICU_VERSION}; attempting to build ICU from scratch...")
      endif()
    endif()
    if (WIN32)
      # not going to attempt to build ICU if we're on Windows for now
      # probably could, but it's more trouble than it's worth I think
      message("-- ICU building not supported on Windows.")
      message(FATAL_ERROR "   -- Please download the latest ICU binaries from http://site.icu-project.org/download")
    elseif(DOWNLOAD_UBUNTU_ICU_BUILD STREQUAL "ON")
      # Be careful this is mostly for github actions
      message("-- Downloading ubuntu icu build for ubuntu 22.04")

      set(ICU_RELEASE "https://github.com/unicode-org/icu/releases/download/release-74-2/icu4c-74_2-Ubuntu22.04-x64.tgz")
      set(ICU_CONTROL "MD5=6786f210e101e0440582ba2d9a057aed")

      FetchContent_Declare(
        ubuntu-icu
        URL ${ICU_RELEASE}
        URL_HASH ${ICU_CONTROL}
      )

      FetchContent_MakeAvailable(ubuntu-icu)

      set(ICU_PREFIX ${PROJECT_BINARY_DIR}/_deps/ubuntu-icu-src/usr/local)

      set(ICU_INCLUDE_DIRS ${ICU_PREFIX}/include)

      set(ICU_DATA_LIBRARY ${ICU_PREFIX}/lib/libicudata.so.74)
      set(ICU_I18N_LIBRARY ${ICU_PREFIX}/lib/libicui18n.so.74)
      set(ICU_UC_LIBRARY ${ICU_PREFIX}/lib/libicuuc.so.74)
      set(ICU_IO_LIBRARY ${ICU_PREFIX}/lib/libicuio.so.74)

      add_library(icudata IMPORTED STATIC GLOBAL)
      set_target_properties(icudata PROPERTIES IMPORTED_LOCATION ${ICU_DATA_LIBRARY})
      target_include_directories(icudata INTERFACE ${ICU_INCLUDE_DIRS})

      add_library(icu18n IMPORTED STATIC GLOBAL)
      set_target_properties(icu18n PROPERTIES IMPORTED_LOCATION ${ICU_I18N_LIBRARY})
      target_include_directories(icu18n INTERFACE ${ICU_INCLUDE_DIRS})

      add_library(icuuc IMPORTED STATIC GLOBAL)
      set_target_properties(icuuc PROPERTIES IMPORTED_LOCATION ${ICU_UC_LIBRARY})
      target_include_directories(icuuc INTERFACE ${ICU_INCLUDE_DIRS})

      add_library(icuio IMPORTED STATIC GLOBAL)
      set_target_properties(icuio PROPERTIES IMPORTED_LOCATION ${ICU_IO_LIBRARY})
      target_include_directories(icuio INTERFACE ${ICU_INCLUDE_DIRS})
      target_link_libraries(icuio INTERFACE icu18n)


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
  target_link_libraries(unicode INTERFACE icuio icuuc icui18n icudata)
  set(ICU_LIBRARIES icui18n icuuc icudata icuio)

  message("-- ICU include dirs: ${ICU_INCLUDE_DIRS}")
  message("-- ICU libraries: ${ICU_LIBRARIES}")


  if (ICU_IS_EXTERNAL)
    file(MAKE_DIRECTORY ${ICU_INCLUDE_DIRS})
  endif()
endfunction()
