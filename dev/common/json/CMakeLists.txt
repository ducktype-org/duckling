add_library(JSON INTERFACE)

target_include_directories(JSON INTERFACE src)
target_link_libraries(JSON INTERFACE Base nlohmann_json)

add_example(json_example examples/example.cpp USES JSON)
