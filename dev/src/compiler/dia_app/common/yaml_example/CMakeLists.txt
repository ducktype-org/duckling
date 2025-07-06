add_executable(yaml_example src/dia_app/yaml_example/main.cpp)

target_link_libraries(yaml_example PUBLIC yaml-cpp::yaml-cpp)