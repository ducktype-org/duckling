add_library(Concurrent
    src/concurrent/module_flags/worker_count.cpp
    src/concurrent/worker/worker.cpp
    src/concurrent/worker/worker_manager.cpp
    src/concurrent/utils/nop_wait.cpp
    src/concurrent/base/collections/hash_map.cpp
)

make_module(Concurrent USES Base)

duck_add_test(common race_tester tests/race_tester_test.cpp USES Concurrent Base)
duck_add_test(common concurrent_module tests/concurrent_module_test.cpp USES Concurrent Base)
duck_add_test(common worker_manager tests/worker_manager_test.cpp USES Concurrent Base)


# bring the test back
# duck_add_test(common task_pool tests/task_pool_test.cpp USES Concurrent Base)
