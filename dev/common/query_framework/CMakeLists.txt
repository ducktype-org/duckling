add_library(QueryFramework
	src/query_framework/detail/logs.cpp
	src/query_framework/detail/dep_graph.cpp
	src/query_framework/detail/query_id_provider.cpp
	src/query_framework/detail/query_id.cpp
	src/query_framework/utils/with_context_do.cpp
)

make_module(QueryFramework USES Base Diagnostic Tester)

add_example(query_example examples/example.cpp USES QueryFramework)

duck_add_test(common query_framework USES QueryFramework)
