#include <concurrent/module_flags/worker_count.hpp>
#include <concurrent/thread_manager/thread_manager.hpp>

#include <tester/tester.hpp>

class ThreadManagerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ThreadManagerTest


public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(basicFunctionalityTest); }

private:
	void basicFunctionalityTest() {
		concurrent::ThreadManager<4> thread_manager;

		auto all_threads = thread_manager.getAllThreads();
		ASSERT_EQUAL(all_threads.size(), 4);

		auto free_threads = thread_manager.getFreeThreads();
		ASSERT_EQUAL(free_threads.size(), 4);

		std::atomic_int counter = 0;

		constexpr u64 TASKS_COUNT = 4;

		auto now = std::chrono::steady_clock::now();

		for (u64 i = 0; i < TASKS_COUNT; i++) {
			thread_manager.scheduleTaskOnAnyFreeThread([&counter]() {
				std::this_thread::sleep_for(std::chrono::milliseconds(1'000));
				counter.fetch_add(1, std::memory_order_relaxed);
			});
		}

		// Wait for all tasks to complete
		while (counter.load(std::memory_order_relaxed) < TASKS_COUNT) std::this_thread::yield();

		auto elapsed    = std::chrono::steady_clock::now() - now;
		auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
		// Since we have 4 threads and 4 tasks that each take 1 second,
		// the total time should be just over 1 second (1000 ms).
		assertTrue(elapsed_ms < 2'000, base::strConcat("Elapsed time: ", elapsed_ms, " ms"));

		ASSERT_EQUAL(counter.load(std::memory_order_relaxed), TASKS_COUNT);
	}
};

TESTER_COMMON_MAIN("/src/common/concurrent/tests/");
