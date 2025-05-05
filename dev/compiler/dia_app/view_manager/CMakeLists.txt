add_executable(view_manager src/dia_app/view_manager/main.cpp)

target_link_libraries(view_manager PUBLIC ViewConstructor grpc++ Proto)