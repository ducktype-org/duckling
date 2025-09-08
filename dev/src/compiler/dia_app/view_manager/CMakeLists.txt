set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w -g3")

target_link_libraries(view_manager PUBLIC JSON yaml-cpp::yaml-cpp grpc++ Proto)

add_library(VIEW_MANAGER
    src/dia_app/view_manager/view_manager.hpp
)

make_module(VIEW_MANAGER USES JSON yaml-cpp::yaml-cpp grpc++ Proto)
