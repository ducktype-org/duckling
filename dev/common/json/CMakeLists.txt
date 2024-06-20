add_library(JSON INTERFACE)

target_include_directories(JSON INTERFACE src)

add_example(json_example examples/example.cpp USES JSON)
