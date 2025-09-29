include(FetchContent)
include(CheckCXXSourceCompiles)

# stdc++exp library (experimental C++ library)
# NOTE: We do not use experimental C++ features directly, but libstdc++ implements <stacktrace>
# in this library, so we need to link it if we want stacktrace support.
add_library(system_stdcxxexp INTERFACE)


check_cxx_source_compiles("
#include <version>

#ifndef _GLIBCXX_RELEASE
    #error libstdc++ not detected
#else
    int main() {}
#endif
" USES_GNU_STDLIB)

if(USES_GNU_STDLIB)
	target_link_libraries(system_stdcxxexp INTERFACE "-lstdc++exp")
endif()
