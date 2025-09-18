set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w -g3")

add_library(VIEW_MANAGER
    src/dia_app/view_manager/view_manager.hpp
)

make_module(VIEW_MANAGER USES JSON yaml-cpp::yaml-cpp grpc++ Proto)
