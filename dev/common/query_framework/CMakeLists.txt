add_library(QueryFramework
		src/query_framework/query_impl.cpp
		src/query_framework/logs.cpp)

make_module(QueryFramework USES Base)

add_example(query_example examples/example.cpp USES QueryFramework)

rift_add_test(common query_test_simple tests/test.cpp
		USES QueryFramework)


