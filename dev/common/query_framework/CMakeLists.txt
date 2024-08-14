add_library(QueryFramework
	src/query_framework/logs.cpp
	src/query_framework/dep_graph.cpp
	src/query_framework/query_id_provider.cpp
	src/query_framework/query_id.cpp
	src/query_framework/test_utils/context_suite.cpp
)

make_module(QueryFramework USES Base Diagnostic Tester)

add_example(query_example examples/example.cpp USES QueryFramework)

rift_add_test(common query_test_simple tests/test.cpp
	USES QueryFramework)
