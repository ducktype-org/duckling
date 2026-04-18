# mimalloc - Microsoft's high-performance allocator

if(${ALLOCATOR} STREQUAL "mimalloc")
    FetchContent_Declare(
        mimalloc
        GIT_REPOSITORY https://github.com/microsoft/mimalloc.git
        GIT_TAG v3.3.0 # Released 16 April 2026
        GIT_SHALLOW TRUE
    )

    set(MI_BUILD_STATIC ON CACHE INTERNAL "")
    
    set(MI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(MI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    
    set(MI_NO_OPT_ARCH ON CACHE INTERNAL "")
    
    # This should override the default allocator globally, so we don't have to worry about it.
    # Note that this might not work on all platforms.
    # This might be useful if mimalloc will not work on different platforms: https://microsoft.github.io/mimalloc/overrides.html
    set(MI_OVERRIDE ON CACHE INTERNAL "")
    
    FetchContent_MakeAvailable(mimalloc)

    target_link_libraries(duckc PRIVATE mimalloc-static)
endif()