#include "concurrent/task_pool/task_pool.hpp"
#include "concurrent/worker/task.hpp"

#include <concurrent/base/collections/hash_map.hpp>
#include <concurrent/module_flags/worker_count.hpp>
#include <concurrent/worker/worker_data.hpp>
#include <concurrent/worker/worker_manager.hpp>

#include <tester/tester.hpp>

using NoTaskCallback = std::function<void(concurrent::WDRef)>;

static std::atomic<std::shared_ptr<NoTaskCallback>> g_no_task_callback{ nullptr };

void noTaskCallback(concurrent::WDRef wd) {
	auto cb = g_no_task_callback.load(std::memory_order_acquire);
	if (cb) (*cb)(wd);
}

void setNoTaskCallback(NoTaskCallback cb) {
	g_no_task_callback.store(
		std::make_shared<NoTaskCallback>(std::move(cb)), std::memory_order_release
	);
}

void clearNoTaskCallback() { g_no_task_callback.store(nullptr, std::memory_order_release); }

void flushWorkers(concurrent::WorkerManager& manager) {
	auto      workers  = manager.getAllWorkers();
	const int expected = static_cast<int>(workers.size());

	auto completed = std::make_shared<std::atomic_int>(0);

	setNoTaskCallback([completed](concurrent::WDRef) {
		completed->fetch_add(1, std::memory_order_relaxed);
	});

	for (auto worker_id: workers) {
		manager.scheduleTaskOnWorker(worker_id, [](concurrent::WDRef) {
			// empty task
		});
	}

	while (completed->load(std::memory_order_relaxed) < expected) std::this_thread::yield();

	clearNoTaskCallback();
}

class TaskPoolTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS TaskPoolTest


public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		concurrent::setWorkerCount(10);
		// TESTER_ADD_TEST(basicFunctionalityTest);
		// TESTER_ADD_TEST(testFibbonaciSchedule);
		// TESTER_ADD_TEST(testFibbonaciQuery);
		TESTER_ADD_TEST(testFibbonaciScheduleAndQuery);
		// TESTER_ADD_TEST(deadlockTest);
	}

private:
	void deadlockTest() {
		// Test designed to trigger potential deadlock scenarios with 2 workers
		concurrent::setWorkerCount(2);

		concurrent::WorkerManager worker_manager([](concurrent::WDRef wd) { noTaskCallback(wd); });
		concurrent::TaskPool      task_pool(worker_manager);
		setNoTaskCallback([&task_pool](concurrent::WDRef) mutable { task_pool.onWorkerNoTasks(); });

		std::atomic_int completed_tasks = 0;


		auto t1 = concurrent::PoolTask(1, [=, &completed_tasks](concurrent::WDRef) {
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			completed_tasks++;
		});

		// Factory for Task 2 (which schedules Task 1)
		auto t2
			= concurrent::PoolTask(2, [t1, &completed_tasks, &task_pool](concurrent::WDRef) mutable {
				  auto handle1 = task_pool.schedule(std::move(t1));
				  handle1.await();
				  completed_tasks++;
			  });


		// Task 3: Schedules Task 2 (which needs T1)
		concurrent::PoolTask task3(3, [t2, &completed_tasks, &task_pool](concurrent::WDRef) mutable {
			auto handle = task_pool.schedule(std::move(t2));
			handle.await();
			completed_tasks++;
		});

		// Task 4 (Root): Schedules Task 2 and Task 3
		concurrent::PoolTask task4(
			4,
			[t2, task3, &completed_tasks, &task_pool](concurrent::WDRef) mutable {
				// Schedule dependencies
				auto handle2 = task_pool.schedule(std::move(t2));
				auto handle3 = task_pool.schedule(std::move(task3));

				handle2.await();
				handle3.await();
				completed_tasks++;
			}
		);


		task_pool.addInitialTasks({ t1, task4 });
		task_pool.startExecution();

		task_pool.waitExecutionCompletion();

		std::cout << "Execution completed.\n";
		flushWorkers(worker_manager);
		clearNoTaskCallback();

		int final_completed = completed_tasks.load();
		// Expect: 4(Root) + 2(T2) + 3(T3) + 1(T1) + 20(T2 nested) + 10(T1 nested) = 6 tasks
		ASSERT_EQUAL(final_completed, 6);
	}

	void basicFunctionalityTest() {
		concurrent::WorkerManager worker_manager(noTaskCallback);
		concurrent::TaskPool      task_pool(worker_manager);
		setNoTaskCallback([&task_pool](concurrent::WDRef) mutable { task_pool.onWorkerNoTasks(); });
		constexpr usize  TASK_COUNT      = 10;
		std::atomic_int  completed_tasks = 0;
		concurrent::Task task            = [&completed_tasks](concurrent::WDRef) {
            std::cout << base::strConcat(
                "Executing task on worker ",
                static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
                "\n"
            );
            completed_tasks.fetch_add(1, std::memory_order_relaxed);
		};
		std::vector<concurrent::PoolTask> tasks;
		tasks.reserve(TASK_COUNT);
		for (usize i = 0; i < TASK_COUNT; ++i) tasks.emplace_back(i, task);

		task_pool.addInitialTasks(std::move(tasks));
		task_pool.startExecution();

		task_pool.waitExecutionCompletion();
		std::cout << "Execution completed.\n";
		ASSERT_EQUAL(completed_tasks.load(std::memory_order_relaxed), TASK_COUNT);
	}

	void testFibbonaciSchedule() {
		concurrent::WorkerManager worker_manager([](concurrent::WDRef wd) { noTaskCallback(wd); });
		concurrent::TaskPool      task_pool(worker_manager);
		setNoTaskCallback([&task_pool](concurrent::WDRef) mutable { task_pool.onWorkerNoTasks(); });

		concurrent::ConHashMap<u64, u64> fib_cache;


		std::function<u64(u64)> fib_task_gen;
		fib_task_gen = [&task_pool, &fib_cache, &fib_task_gen](u64 n) -> u64 {
			std::cout << base::strConcat(
				"Worker ",
				static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
				" computing fib(",
				n,
				")\n"
			);
			if (n <= 1) {
				fib_cache.put(n, n);
				return n;
			}

			concurrent::PoolTask task1(n - 1, [n, &fib_task_gen](concurrent::WDRef) {
				return fib_task_gen(n - 1);
			});

			concurrent::PoolTask task2(n - 2, [n, &fib_task_gen](concurrent::WDRef) {
				return fib_task_gen(n - 2);
			});

			auto future1 = task_pool.schedule(std::move(task1));
			auto future2 = task_pool.schedule(std::move(task2));

			future1.await();
			future2.await();

			auto value1 = fib_cache.getCopy(n - 1);
			auto value2 = fib_cache.getCopy(n - 2);

			u64 result = (value1 + value2) % static_cast<u64>(1e9 + 7);
			fib_cache.put(n, result);
			std::cout << base::strConcat("...");
			return result;
		};

		concurrent::PoolTask initial_task(250, [&fib_task_gen](concurrent::WDRef) {
			return fib_task_gen(250);
		});

		task_pool.addInitialTasks({ std::move(initial_task) });
		task_pool.startExecution();

		task_pool.waitExecutionCompletion();

		std::cout << "Execution completed.\n";

		flushWorkers(worker_manager);

		clearNoTaskCallback();
	}

	void testFibbonaciQuery() {
		concurrent::WorkerManager worker_manager([](concurrent::WDRef wd) { noTaskCallback(wd); });
		concurrent::TaskPool      task_pool(worker_manager);
		setNoTaskCallback([&task_pool](concurrent::WDRef) mutable { task_pool.onWorkerNoTasks(); });

		concurrent::ConHashMap<u64, u64> fib_cache;


		std::function<u64(u64)> fib_task_gen;
		fib_task_gen = [&task_pool, &fib_cache, &fib_task_gen](u64 n) -> u64 {
			std::cout << base::strConcat(
				"Worker ",
				static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
				" computing fib(",
				n,
				")\n"
			);
			if (n <= 1) {
				fib_cache.put(n, n);
				return n;
			}

			concurrent::PoolTask task1(n - 1, [n, &fib_task_gen](concurrent::WDRef) {
				return fib_task_gen(n - 1);
			});

			concurrent::PoolTask task2(n - 2, [n, &fib_task_gen](concurrent::WDRef) {
				return fib_task_gen(n - 2);
			});

			task_pool.query(task1);
			task_pool.query(task2);

			auto value1 = fib_cache.getCopy(n - 1);
			auto value2 = fib_cache.getCopy(n - 2);

			u64 result = (value1 + value2) % static_cast<u64>(1e9 + 7);
			fib_cache.put(n, result);
			std::cout << base::strConcat("...");
			return result;
		};

		concurrent::PoolTask initial_task(250, [&fib_task_gen](concurrent::WDRef) {
			return fib_task_gen(250);
		});

		task_pool.addInitialTasks({ std::move(initial_task) });
		task_pool.startExecution();

		task_pool.waitExecutionCompletion();

		std::cout << "Execution completed.\n";

		flushWorkers(worker_manager);

		clearNoTaskCallback();
	}

	void testFibbonaciScheduleAndQuery() {
		concurrent::WorkerManager worker_manager([](concurrent::WDRef wd) { noTaskCallback(wd); });
		concurrent::TaskPool      task_pool(worker_manager);
		setNoTaskCallback([&task_pool](concurrent::WDRef) mutable { task_pool.onWorkerNoTasks(); });

		concurrent::ConHashMap<u64, u64> fib_cache;


		std::function<u64(u64)> fib_task_gen;
		fib_task_gen = [&task_pool, &fib_cache, &fib_task_gen](u64 n) -> u64 {
			std::cout << base::strConcat(
				"Worker ",
				static_cast<usize>(concurrent::Worker::getCurrentWorkerID()),
				" computing fib(",
				n,
				")\n"
			);
			if (n <= 1) {
				fib_cache.put(n, n);
				return n;
			}

			concurrent::PoolTask task1(n - 1, [n, &fib_task_gen](concurrent::WDRef) {
				return fib_task_gen(n - 1);
			});

			concurrent::PoolTask task2(n - 2, [n, &fib_task_gen](concurrent::WDRef) {
				return fib_task_gen(n - 2);
			});

			auto future1 = task_pool.schedule(std::move(task1));
			task_pool.query(task2);
			future1.await();

			auto value1 = fib_cache.getCopy(n - 1);
			auto value2 = fib_cache.getCopy(n - 2);

			u64 result = (value1 + value2) % static_cast<u64>(1e9 + 7);
			fib_cache.put(n, result);
			std::cout << base::strConcat("...");
			return result;
		};

		concurrent::PoolTask initial_task(250, [&fib_task_gen](concurrent::WDRef) {
			return fib_task_gen(250);
		});

		task_pool.addInitialTasks({ std::move(initial_task) });
		task_pool.startExecution();

		task_pool.waitExecutionCompletion();

		std::cout << "Execution completed.\n";

		flushWorkers(worker_manager);

		clearNoTaskCallback();
	}
};

TESTER_COMMON_MAIN("/src/common/concurrent/tests/");
