add_library(JSON
	src/json/diagnostics.cpp
	src/json/extract.cpp
)

make_module(JSON USES Base StringID nlohmann_json::nlohmann_json)

add_example(json_example examples/example.cpp USES JSON)

duck_add_test(common json_extract tests/json_extract_test.cpp USES JSON)
