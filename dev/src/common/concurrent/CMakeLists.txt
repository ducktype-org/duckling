add_library(Concurrent
    src/concurrent/module_flags/worker_count.cpp
    src/concurrent/workers/worker_data.cpp
    src/concurrent/utils/nop_wait.cpp
)

find_package(mimalloc 2.2 REQUIRED)

make_module(Concurrent USES Base mimalloc)

duck_add_test(common concurrent_module tests/concurrent_module_test.cpp USES Concurrent Base)


find_package(TBB REQUIRED)

add_example(concurrent_example examples/concurrent_example.cpp USES Concurrent Base TBB::tbb)

