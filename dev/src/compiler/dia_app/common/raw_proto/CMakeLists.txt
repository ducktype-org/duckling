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


# -----------------------------
# Explicit command to generate the files for manual Proto library
# -----------------------------

# List of generated files
set(GENERATED_PROTO_FILES
    "${PROTO_BINARY_DIR}/view.pb.cc"
    "${PROTO_BINARY_DIR}/view.pb.h"
    "${PROTO_BINARY_DIR}/view.grpc.pb.cc"
    "${PROTO_BINARY_DIR}/view.grpc.pb.h"
)

# Make sure output directory exists
file(MAKE_DIRECTORY "${PROTO_BINARY_DIR}")

# Custom target that depends on all generated files
add_custom_target(generate_proto_files ALL
    DEPENDS ${GENERATED_PROTO_FILES}
)

# C++ generation
add_custom_command(
    OUTPUT "${PROTO_BINARY_DIR}/view.pb.cc" "${PROTO_BINARY_DIR}/view.pb.h"
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --cpp_out="${PROTO_BINARY_DIR}" -I "${PROTO_IMPORT_DIRS}" "${CMAKE_CURRENT_LIST_DIR}/view.proto"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/view.proto"
    COMMENT "Generating C++ protobuf files"
)

# gRPC generation
add_custom_command(
    OUTPUT "${PROTO_BINARY_DIR}/view.grpc.pb.cc" "${PROTO_BINARY_DIR}/view.grpc.pb.h"
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --grpc_out="${PROTO_BINARY_DIR}"
         --plugin=protoc-gen-grpc=$<TARGET_FILE:grpc_cpp_plugin>
         -I "${PROTO_IMPORT_DIRS}"
         "${CMAKE_CURRENT_LIST_DIR}/view.proto"
    DEPENDS "${CMAKE_CURRENT_LIST_DIR}/view.proto"
    COMMENT "Generating gRPC C++ files"
)