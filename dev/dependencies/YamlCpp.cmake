include(FetchContent)
set(yaml_cpp_TAG "yaml-cpp-0.9.0")  # released 2026-02-04


FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 	     ${yaml_cpp_TAG}
  SYSTEM
)
FetchContent_MakeAvailable(yaml-cpp)

target_compile_options(yaml-cpp PRIVATE
    -Wno-error=conversion
    -Wno-conversion
)
