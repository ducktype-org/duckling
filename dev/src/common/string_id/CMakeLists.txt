add_library(StringID
	src/string_id/string_id.cpp
)

make_module(StringID USES Base Hashing Concurrent)

add_example(string_id_example examples/string_id_example.cpp USES StringID)

duck_add_test(common string_id tests/string_id_test.cpp USES StringID)
