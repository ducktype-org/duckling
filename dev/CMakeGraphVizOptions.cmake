# This has to be a separate file named CMakeGraphVizOptions.cmake
# due to how `CMake --graphviz` command works.
# See https://cmake.org/cmake/help/latest/manual/cmake.1.html#cmdoption-cmake-graphviz for more info.


# Add target information to the dependency graph when using --graphviz option
SET(GRAPHVIZ_CUSTOM_TARGETS TRUE)
