# This assumes that these files are generated outside of the cmake pipline.
add_library(Proto
	src/proto/view.grpc.pb.cc
	src/proto/view.grpc.pb.h
	src/proto/view.pb.cc
	src/proto/view.pb.h
)

target_include_directories(Proto
  PUBLIC
    ${Protobuf_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(Proto
  PUBLIC
    protobuf::libprotobuf
    grpc++
)

make_module(Proto USES)