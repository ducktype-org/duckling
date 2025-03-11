add_library(Hashing
    src/hashing/type_unique_code.cpp
)

make_module(Hashing USES Base)

add_example(hash_example examples/example.cpp USES Hashing Base)

duck_add_test(common hashing_coverage USES Hashing Base Tester)

duck_add_test(common hashing USES Hashing Base Tester)
