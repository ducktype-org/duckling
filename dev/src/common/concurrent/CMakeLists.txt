add_library(Concurrent
    src/concurrent/module_flags/worker_count.cpp
    src/concurrent/workers/worker_data.cpp
    src/concurrent/utils/nop_wait.cpp
    src/concurrent/thread_manager/thread.cpp
)

make_module(Concurrent USES Base)

duck_add_test(common concurrent_module tests/concurrent_module_test.cpp USES Concurrent Base)
duck_add_test(common thread_manager tests/thread_manager_test.cpp USES Concurrent Base)
