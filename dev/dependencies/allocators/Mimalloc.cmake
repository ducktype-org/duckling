# This file includes the necessary CMake code to use
# mimalloc – Microsoft's high-performance allocator – as a custom
# binary-wide allocator.
#
# For more information about mimalloc, see:
# * https://github.com/microsoft/mimalloc
# * https://microsoft.github.io/mimalloc/
#
#
# Important note about this file:
#
# There are two main ways the above can be achieved:
# 1. Install mimalloc locally and find it with find_package.
#    This would require some automation for this process, likely similar to the one we have for local LLVM builds.
# 2. Use FetchContent_Declare to "import" mimalloc directly into our build.
#
# Both approaches have their pros and cons related mostly to CMake and ways
# to override the default allocator (see https://microsoft.github.io/mimalloc/overrides.html for more info on this topic).
#
# The second approach was chosen as the default/fallback, mainly because it is the simpler one for us currently,
# it is also done this way by other projects and because it is simpler to use by the end user (developer).
# However, if a system-installed mimalloc (>= 2.0) is found via find_package, it will be preferred.
#
#
# Important note about the chosen approach:
#
# Using FetchContent_Declare effectively means that mimalloc will be built as part of our project, which has some implications, importantly:
# * it will use the same build type (DevOpt, Release, Perf, etc.),
# * it will use the same config variables that we set for our project (compiler, compiler options, custom defined, etc.).
#
# This is problematic, as mimallocs' CMake configuration assumes Release or Debug build types,
# different warning levels, different set of custom defines, etc.
# For this reason we have to do a little bit of fragile cmake-hacking (see notes in the code below) to make it work,
# and importantly, to make the build properly optimized and performant.
#
# I think I did this correctly, but if we see any performance issues related to the allocator, this is the first place I would check.
# I mostly just read the mimalloc CMakeLists.txt and tried to replicate its release build behavior, but there might be things that I missed.



include(FetchContent)

if("${ALLOCATOR}" STREQUAL "MIMALLOC")
    message(STATUS "Using mimalloc allocator")

    # First, try to find a system-installed mimalloc of a recent-enough version.
    find_package(mimalloc 3.0 QUIET)

    if(mimalloc_FOUND)
        message(STATUS "Found system mimalloc ${mimalloc_VERSION}, using it.")

        # Finally, link our proxy library to mimalloc, so we can use it in our project.
        target_link_libraries(allocator_proxy_library INTERFACE mimalloc)
    else()
        message(STATUS "System mimalloc not found or too old, building from source.")

        FetchContent_Declare(
            mimalloc
            GIT_REPOSITORY https://github.com/microsoft/mimalloc.git
            GIT_TAG v3.3.0 # Released 16 April 2026
            GIT_SHALLOW TRUE
        )

        # Only build the mimalloc as static library.
        set(MI_BUILD_STATIC  ON  CACHE INTERNAL "" FORCE)
        set(MI_BUILD_SHARED  OFF CACHE INTERNAL "" FORCE)
        set(MI_BUILD_TESTS   OFF CACHE INTERNAL "" FORCE)

        # Turn off any mimmalloc debug features.
        # Note that mimalloc is capable of providing a lot of statistics information,
        # not sure if these options impact its ability to do so, but we can always turn them on in the future if needed.
        set(MI_DEBUG          OFF CACHE INTERNAL "" FORCE)
        set(MI_DEBUG_INTERNAL OFF CACHE INTERNAL "" FORCE)
        set(MI_DEBUG_FULL     OFF CACHE INTERNAL "" FORCE)

        # Keep mimalloc's architecture-specific optimizations enabled.
        set(MI_NO_OPT_ARCH   OFF CACHE INTERNAL "" FORCE)

        # Compile mimalloc using the C compiler.
        # Based on my tests, using the C++ compiler also works.
        set(MI_USE_CXX       OFF CACHE INTERNAL "" FORCE)

        # Override the default allocator globally, so we don't have to worry about it.
        # Note that this might not work on all platforms.
        # See also https://microsoft.github.io/mimalloc/overrides.html
        set(MI_OVERRIDE ON CACHE INTERNAL "" FORCE)

        FetchContent_MakeAvailable(mimalloc)

        # Turn off LINK_LIBRARIES_ONLY_TARGETS for mimalloc, otherwise the build fails as mimalloc links to -lpthread.
        set_target_properties(mimalloc-static PROPERTIES LINK_LIBRARIES_ONLY_TARGETS OFF)

        # Add custom compile options to mimalloc.
        # Note mimalloc's CMakeLists.txt sets different compile options for different build types,
        # but our project uses custom build types (DevOpt, Perf, etc.) that do not necessarily match the ones expected by mimalloc (Debug, Release).
        # For this reason, we have to set the compile options manually here.
        # See also the note at the top of this file.
        #
        # The options:
        # - -O3 -- always optimize
        # - -DNDEBUG -- disable debug features
        # - -w -- disable warnings for mimalloc, otherwise the build fails with our strict Werror warnings.
        # - -DMI_BUILD_RELEASE and -DMI_CMAKE_BUILD_TYPE=release -- these are set by mimalloc's CMakeLists.txt for release builds. Not sure how they impact mimalloc's behavior.
        target_compile_options(mimalloc-static PRIVATE "-O3" "-DNDEBUG" "-w" "-DMI_BUILD_RELEASE" "-DMI_CMAKE_BUILD_TYPE=release")


        # Finally, link our proxy library to mimalloc, so we can use it in our project.
        target_link_libraries(allocator_proxy_library INTERFACE mimalloc-static)
    endif()
endif()
