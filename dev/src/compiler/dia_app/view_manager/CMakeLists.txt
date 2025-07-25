set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w -g3") # TODO: This should be more isolated

add_executable(view_manager
    src/dia_app/view_manager/components.cpp
    src/dia_app/view_manager/components.hpp
    src/dia_app/view_manager/dia_parser.hpp
    src/dia_app/view_manager/diagnostics.cpp
    src/dia_app/view_manager/diagnostics.hpp
    src/dia_app/view_manager/display_elements.cpp
    src/dia_app/view_manager/display_elements.hpp
    src/dia_app/view_manager/main.cpp
    src/dia_app/view_manager/side_panel.cpp
    src/dia_app/view_manager/side_panel.hpp
    src/dia_app/view_manager/template_application.hpp
    src/dia_app/view_manager/template_elements.hpp
    src/dia_app/view_manager/template_parser.hpp
    src/dia_app/view_manager/utils.cpp
    src/dia_app/view_manager/utils.hpp
    src/dia_app/view_manager/view_constructor.hpp
    src/dia_app/view_manager/view_manager.cpp
    src/dia_app/view_manager/view_manager.hpp
)
add_executable(view_constructor_test
    src/dia_app/view_manager/components.cpp
    src/dia_app/view_manager/components.hpp
    src/dia_app/view_manager/dia_parser.hpp
    src/dia_app/view_manager/display_elements.cpp
    src/dia_app/view_manager/display_elements.hpp
    src/dia_app/view_manager/template_application.hpp
    src/dia_app/view_manager/template_elements.hpp
    src/dia_app/view_manager/template_parser.hpp
    src/dia_app/view_manager/utils.cpp
    src/dia_app/view_manager/utils.hpp
    src/dia_app/view_manager/view_constructor.hpp
    src/dia_app/view_manager/vc_test.cpp
)

add_executable(yaml_test
    src/dia_app/view_manager/components.cpp
    src/dia_app/view_manager/dia_parser.hpp
    src/dia_app/view_manager/display_elements.cpp
    src/dia_app/view_manager/display_elements.hpp
    src/dia_app/view_manager/template_application.hpp
    src/dia_app/view_manager/template_elements.hpp
    src/dia_app/view_manager/template_parser.hpp
    src/dia_app/view_manager/utils.cpp
    src/dia_app/view_manager/utils.hpp
    src/dia_app/view_manager/view_constructor.hpp
    src/dia_app/view_manager/vm_yaml_test.cpp
)

target_link_libraries(view_manager PUBLIC JSON yaml-cpp::yaml-cpp grpc++ Proto)
target_link_libraries(view_constructor_test PUBLIC JSON yaml-cpp::yaml-cpp Proto)
target_link_libraries(yaml_test PUBLIC JSON yaml-cpp::yaml-cpp Proto)

add_library(VIEW_MANAGER
    src/dia_app/view_manager/components.cpp
    src/dia_app/view_manager/components.hpp
    src/dia_app/view_manager/dia_parser.hpp
    src/dia_app/view_manager/diagnostics.cpp
    src/dia_app/view_manager/diagnostics.hpp
    src/dia_app/view_manager/display_elements.cpp
    src/dia_app/view_manager/display_elements.hpp
    src/dia_app/view_manager/side_panel.cpp
    src/dia_app/view_manager/side_panel.hpp
    src/dia_app/view_manager/template_application.hpp
    src/dia_app/view_manager/template_elements.hpp
    src/dia_app/view_manager/template_parser.hpp
    src/dia_app/view_manager/utils.cpp
    src/dia_app/view_manager/utils.hpp
    src/dia_app/view_manager/view_constructor.hpp
    src/dia_app/view_manager/view_manager.cpp
    src/dia_app/view_manager/view_manager.hpp
)

make_module(VIEW_MANAGER USES JSON yaml-cpp::yaml-cpp grpc++ Proto)
