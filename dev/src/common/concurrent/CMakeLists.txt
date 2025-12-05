add_library(Concurrent
    src/concurrent/module_flags/worker_count.cpp
)

make_module(Concurrent USES Base)

add_example(map_test examples/map_test.cpp
        USES Base Concurrent)

target_sources(map_test PRIVATE
        examples/work.cpp)
