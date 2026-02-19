add_library(Hashing
	src/hashing/component_hash.cpp
)

llvm_map_components_to_libnames(llvm_libs
    Support)
target_include_directories(Hashing PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(Hashing ${llvm_libs})

make_module(Hashing USES Base StringID llvm_helpers)

add_example(hash_example examples/example.cpp USES Hashing Base)

duck_add_test(common hashing_coverage tests/hashing_coverage_test.cpp USES Hashing Base Tester)

duck_add_test(common hashing tests/hashing_test.cpp USES Hashing Base Tester)
