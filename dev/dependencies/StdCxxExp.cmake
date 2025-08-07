# stdc++exp library (experimental C++ library)
# NOTE: We do not use experimental C++ features directly, but GCC implements <stacktrace>
# in this library, so we need to link it if we want stacktrace support.
add_library(system_stdcxxexp INTERFACE)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(system_stdcxxexp INTERFACE "-lstdc++exp")
endif()