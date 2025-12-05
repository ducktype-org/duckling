add_library(Concurrent
    src/concurrent/module_flags/worker_count.cpp
)

make_module(Concurrent USES Base)


