include(FetchContent)
set(yaml_cpp_TAG "65c1c270dbe7eec37b2df2531d7497c4eea79aee")


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
