add_library(Hashing
    src/hashing/type_unique_code.cpp
)

make_module(Hashing USES Base)

add_example(hash_example examples/example.cpp USES Hashing Base)

duck_add_test(common hashing_test_simple tests/test.cpp
	USES Hashing Base Tester)

duck_add_test(common hashing_test_coverage tests/coverage.cpp
    USES Hashing Base Tester)
