add_library(QueryFramework
        src/query_framework/frame.cpp)

make_module(QueryFramework USES Base)

rift_add_test(common query_test_simple tests/test.cpp
		USES QueryFramework)


