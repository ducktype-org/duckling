add_library(DucklingDefinitions
	src/duckling_definitions/key_spec_op.cpp
	src/duckling_definitions/operator_precedence.cpp
)

make_module(DucklingDefinitions USES Base)

duckling_add_test(common duckling_def_test_simple tests/test.cpp
	USES DucklingDefinitions)
