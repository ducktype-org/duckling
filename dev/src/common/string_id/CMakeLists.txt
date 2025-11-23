add_library(StringID
	src/string_id/string_id.cpp
)

make_module(StringID USES Base)

duck_add_test(common string_id tests/string_id_test.cpp USES StringID)
