if(NOT COMMAND protobuf_generate)
  include(${protobuf_SOURCE_DIR}/cmake/protobuf-generate.cmake)
endif()

add_library(dia_compile_proto OBJECT "${CMAKE_CURRENT_LIST_DIR}/view.proto")

target_link_libraries(dia_compile_proto PUBLIC protobuf::libprotobuf)
target_link_libraries(dia_compile_proto PUBLIC grpc++)

set(PROTO_BINARY_DIR "${CMAKE_CURRENT_LIST_DIR}/../proto/src/proto")
set(PROTO_IMPORT_DIRS "${CMAKE_CURRENT_LIST_DIR}")

target_include_directories(dia_compile_proto PUBLIC "$<BUILD_INTERFACE:${PROTO_BINARY_DIR}>")

protobuf_generate(
    TARGET dia_compile_proto
    IMPORT_DIRS ${PROTO_IMPORT_DIRS}
    PROTOC_OUT_DIR "${PROTO_BINARY_DIR}")

protobuf_generate(
    TARGET dia_compile_proto
    LANGUAGE grpc
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
    PLUGIN "protoc-gen-grpc=\$<TARGET_FILE:grpc_cpp_plugin>"
    IMPORT_DIRS ${PROTO_IMPORT_DIRS}
    PROTOC_OUT_DIR "${PROTO_BINARY_DIR}")