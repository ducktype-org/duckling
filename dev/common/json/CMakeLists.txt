add_library(JSON src/json/helper.cpp)

make_module(JSON USES Base)

add_example(json_example examples/example.cpp USES JSON)
