add_library(Hashing
	src/hashing/component_hash.cpp
)

make_module(Hashing USES Base StringID)

add_example(hash_example examples/example.cpp USES Hashing Base)

duck_add_test(common hashing_coverage tests/hashing_coverage_test.cpp USES Hashing Base Tester)

duck_add_test(common hashing tests/hashing_test.cpp USES Hashing Base Tester)
