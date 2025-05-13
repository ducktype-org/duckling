add_executable(view_manager
    src/dia_app/view_manager/components.cpp
    src/dia_app/view_manager/display_elements.cpp
    src/dia_app/view_manager/main.cpp
    src/dia_app/view_manager/utils.cpp
)
add_executable(view_constructor_test
    src/dia_app/view_manager/display_elements.cpp
    src/dia_app/view_manager/utils.cpp
    src/dia_app/view_manager/vc_test.cpp
)

target_link_libraries(view_manager PUBLIC JSON grpc++ Proto)
target_link_libraries(view_constructor_test PUBLIC JSON)