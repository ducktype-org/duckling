# mimalloc - Microsoft's high-performance allocator
include(FetchContent)


if(${ALLOCATOR} STREQUAL "MIMALLOC")
    message(STATUS "Using mimalloc allocator")

    FetchContent_Declare(
        mimalloc
        GIT_REPOSITORY https://github.com/microsoft/mimalloc.git
        GIT_TAG v3.3.0 # Released 16 April 2026
        GIT_SHALLOW TRUE
    )

    set(MI_BUILD_STATIC ON CACHE INTERNAL "" FORCE)
    set(MI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(MI_BUILD_TESTS OFF CACHE BOOL "" FORCE)

    set(MI_NO_OPT_ARCH OFF CACHE INTERNAL "" FORCE)
    set(MI_USE_CXX OFF CACHE INTERNAL "" FORCE)
    
    # This should override the default allocator globally, so we don't have to worry about it.
    # Note that this might not work on all platforms.
    # This might be useful if mimalloc will not work on different platforms: https://microsoft.github.io/mimalloc/overrides.html
    set(MI_OVERRIDE ON CACHE INTERNAL "" FORCE)

    set(MI_DEBUG OFF CACHE BOOL "" FORCE)
    set(MI_DEBUG_INTERNAL OFF CACHE BOOL "" FORCE)
    set(MI_DEBUG_FULL OFF CACHE BOOL "" FORCE)

    
    FetchContent_MakeAvailable(mimalloc)

    
    # allow LINK_LIBRARIES_ONLY_TARGETS to work with mimalloc, which is needed for allocator_library to work as an interface library.
    # add compile options to mimalloc to disable some warnings that we treat as errors in our project, but mimalloc doesn't.
    set_target_properties(mimalloc-static PROPERTIES LINK_LIBRARIES_ONLY_TARGETS OFF)
    target_compile_options(mimalloc-static PRIVATE "-O3" "-DNDEBUG" "-w" "-DMI_BUILD_RELEASE" "-DMI_CMAKE_BUILD_TYPE=release -DMI_BUILD_RELEASE")


    target_link_libraries(allocator_library INTERFACE mimalloc-static)
endif()
