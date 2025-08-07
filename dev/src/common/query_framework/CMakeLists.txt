add_library(QueryFramework
	src/query_framework/internal/utils/logs.cpp
	src/query_framework/internal/query_graph/query_graph.cpp
	src/query_framework/internal/query_graph/query_state.cpp
	src/query_framework/internal/query_data/query_id.cpp
	src/query_framework/utils/with_context_do.cpp
	src/query_framework/context.cpp

	src/query_framework/query_result.hpp
)

make_module(QueryFramework USES Base Diagnostic ctv)

add_example(query_example examples/example.cpp USES QueryFramework)

duck_add_test(common query_framework tests/query_framework_test.cpp USES QueryFramework)
