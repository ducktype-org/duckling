include(FetchContent)
set(yaml_cpp_TAG "0.8.0")


FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 	     ${yaml_cpp_TAG}
)
FetchContent_MakeAvailable(yaml-cpp)

target_compile_options(yaml-cpp PRIVATE
    -Wno-error=conversion
    -Wno-conversion
)
