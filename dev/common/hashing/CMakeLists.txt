add_library(Hashing
    src/hashing/hash.hpp
    src/hashing/addToHash.hpp
    src/hashing/type_hash_code.hpp
    src/hashing/hashing_algorithms.hpp
    src/hashing/CallOverloads_utils.hpp
    src/hashing/hash_algorithm_utils.hpp
    src/hashing/type_hash_code_def.hpp
)

make_module(Hashing USES Base Tester)

add_example(hash_example examples/example.cpp USES Hashing Base)

duck_add_test(common hashing_test_simple tests/test.cpp
	USES Hashing Base)