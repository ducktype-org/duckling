add_library(Hashing
    src/hashing/hash.hpp
    src/hashing/add_to_hash.hpp
    src/hashing/type_code.hpp
    src/hashing/hashing_algorithms.hpp
    src/hashing/hash_algorithm_utils.hpp
    src/hashing/type_hash_code.hpp
    src/hashing/type_unique_code.hpp
)

make_module(Hashing USES Base)

add_example(hash_example examples/example.cpp USES Hashing Base)

duck_add_test(common hashing_test_simple tests/test.cpp
	USES Hashing Base Tester)

duck_add_test(common hashing_test_coverage tests/coverage.cpp
    USES Hashing Base Tester)
