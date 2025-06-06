add_library(QueryFramework
	src/query_framework/detail/utils/logs.cpp
	src/query_framework/detail/query_graph/query_graph.cpp
	src/query_framework/detail/query_data/query_id.cpp
	src/query_framework/utils/with_context_do.cpp
	src/query_framework/context.cpp
)

make_module(QueryFramework USES Base Diagnostic Hashing)

add_example(query_example examples/example.cpp USES QueryFramework)

duck_add_test(common query_framework tests/query_framework_test.cpp USES QueryFramework)
