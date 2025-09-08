set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w -g3")
set(gRPC_TAG "v1.71.0")
set(yaml_cpp_TAG "0.8.0")
set(rang_TAG "v3.2")

set(CMAKE_LINK_LIBRARIES_ONLY_TARGETS OFF)
# Fixes issues with absl dependencies: https://github.com/protocolbuffers/protobuf/issues/12185
set(ABSL_ENABLE_INSTALL ON)
FetchContent_Declare(
  gRPC
  GIT_REPOSITORY https://github.com/grpc/grpc
  GIT_TAG        ${gRPC_TAG}
)
set(CMAKE_LINK_LIBRARIES_ONLY_TARGETS ON)

FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 	     ${yaml_cpp_TAG}
)

FetchContent_Declare(
  rang
  GIT_REPOSITORY https://github.com/agauniyal/rang.git
  GIT_TAG 	     ${rang_TAG}
)

set(CMAKE_LINK_LIBRARIES_ONLY_TARGETS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
FetchContent_MakeAvailable(gRPC)
set(CMAKE_LINK_LIBRARIES_ONLY_TARGETS ON)

FetchContent_MakeAvailable(yaml-cpp)

FetchContent_MakeAvailable(rang)