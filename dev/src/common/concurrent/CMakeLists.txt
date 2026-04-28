add_library(Concurrent
    src/concurrent/module_flags/worker_count.cpp
    src/concurrent/worker/worker.cpp
    src/concurrent/worker/worker_manager.cpp
    src/concurrent/utils/nop_wait.cpp
    src/concurrent/base/collections/hash_map.cpp
)

make_module(Concurrent USES Base absl::flat_hash_map)

duck_add_test(common race_tester tests/race_tester_test.cpp USES Concurrent Base)
duck_add_test(common concurrent_hash_map tests/concurrent_hash_map_test.cpp USES Concurrent Base)
duck_add_test(common absl tests/absl_test.cpp USES Concurrent Base)
duck_add_test(common concurrent_queue tests/concurrent_queue_test.cpp USES Concurrent Base)
duck_add_test(common worker_manager tests/worker_manager_test.cpp USES Concurrent Base)
