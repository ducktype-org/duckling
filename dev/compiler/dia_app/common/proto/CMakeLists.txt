add_library(Proto
	src/proto/view.grpc.pb.cc
	src/proto/view.grpc.pb.h
	src/proto/view.pb.cc
	src/proto/view.pb.h
)
# TODO: This doesn't build if generated files are not present
# and we get a chicken-and-egg problem, because we need to be able
# to build the whole project in order to be able to generate them.

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