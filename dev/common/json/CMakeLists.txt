add_library(JSON
	src/json/helper.hpp
	src/json/helper.cpp
)

make_module(JSON USES Base json_struct)

add_example(json_example examples/example.cpp USES JSON)
