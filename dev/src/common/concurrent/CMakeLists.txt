add_library(Concurrent
    src/concurrent/module_flags/worker_count.cpp
    src/concurrent/workers/worker_data.cpp
)

make_module(Concurrent USES Base gtl)

add_example(map_perf_test examples/map_test.cpp
        USES Base Concurrent)

target_sources(map_perf_test PRIVATE
        examples/work.cpp)
