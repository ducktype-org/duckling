#include <concurrent/base/collections/hash_map.hpp>
#include <concurrent/base/run_or_timeout.hpp>
#include <concurrent/module_flags/worker_count.hpp>
#include <concurrent/task_pool/task_pool.hpp>
#include <concurrent/worker/worker.hpp>
#include <concurrent/worker/worker_manager.hpp>

#include <tester/tester.hpp>

class TaskPoolTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS TaskPoolTest


public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(basicFunctionalityTest);
		TESTER_ADD_TEST(testFibbonaciSchedule);
		TESTER_ADD_TEST(testFibbonaciScheduleReversed);
		TESTER_ADD_TEST(testFibbonaciQuery);
		TESTER_ADD_TEST(testFibbonaciScheduleAndQuery);
		TESTER_ADD_TEST(testGibbonaci);
	}

protected:
	void beforeAll() override { concurrent::worker::setWorkerCount(10); }

	void fail(std::string_view err, bool critical = true) override {
		concurrent::runOrTimeout(
			concurrent::worker::WorkerManager::get().testPrivateAccessReloadState,
			[&] {
				message(
					base::strConcat("WorkerManager reload state timed out during fail(). "
			                        "Terminating.")
				);
			}
		);
		tester::TestSuite::fail(err, critical);
	}

private:
	void basicFunctionalityTest() {
		concurrent::pool::TaskPool task_pool;
		constexpr usize            TASK_COUNT      = 10;
		std::atomic_int            completed_tasks = 0;
		concurrent::worker::Task   task            = [&completed_tasks](concurrent::worker::WRef) {
            completed_tasks.fetch_add(1, std::memory_order_relaxed);
		};
		std::vector<concurrent::pool::Task> tasks;
		tasks.reserve(TASK_COUNT);
		for (usize i = 0; i < TASK_COUNT; ++i) tasks.emplace_back(i, task);

		task_pool.addInitialTasks(std::move(tasks));
		task_pool.execute();
		task_pool.waitExecutionCompletion();
		std::cout << "Execution completed.\n";
		ASSERT_EQUAL(completed_tasks.load(std::memory_order_relaxed), TASK_COUNT);
	}

	void testFibbonaciSchedule() {
		concurrent::pool::TaskPool task_pool;

		concurrent::ConHashMap<u64, u64> fib_cache;
		std::function<u64(u64)>          fib_task_gen;
		fib_task_gen = [&task_pool, &fib_cache, &fib_task_gen](u64 n) -> u64 {
			if (n <= 1) {
				fib_cache.put(n, n);
				return n;
			}

			concurrent::pool::Task task1(n - 1, [n, &fib_task_gen](concurrent::worker::WRef) {
				return fib_task_gen(n - 1);
			});

			concurrent::pool::Task task2(n - 2, [n, &fib_task_gen](concurrent::worker::WRef) {
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
			return result;
		};

		concurrent::pool::Task initial_task(250, [&fib_task_gen](concurrent::worker::WRef) {
			return fib_task_gen(250);
		});

		concurrent::runOrTimeout(
			[&]() {
				task_pool.addInitialTasks({ std::move(initial_task) });
				task_pool.execute();
				task_pool.waitExecutionCompletion();
			},
			[&] { fail("Timeout"); }
		);
		std::cout << "Execution completed.\n";
	}

	void testFibbonaciScheduleReversed() {
		concurrent::pool::TaskPool task_pool;

		concurrent::ConHashMap<u64, u64> fib_cache;
		std::function<u64(u64)>          fib_task_gen;
		fib_task_gen = [&task_pool, &fib_cache, &fib_task_gen](u64 n) -> u64 {
			if (n <= 1) {
				fib_cache.put(n, n);
				return n;
			}

			concurrent::pool::Task task1(n - 1, [n, &fib_task_gen](concurrent::worker::WRef) {
				return fib_task_gen(n - 1);
			});

			concurrent::pool::Task task2(n - 2, [n, &fib_task_gen](concurrent::worker::WRef) {
				return fib_task_gen(n - 2);
			});

			auto future1 = task_pool.schedule(std::move(task1));
			auto future2 = task_pool.schedule(std::move(task2));

			future2.await();
			future1.await();

			auto value1 = fib_cache.getCopy(n - 1);
			auto value2 = fib_cache.getCopy(n - 2);

			u64 result = (value1 + value2) % static_cast<u64>(1e9 + 7);
			fib_cache.put(n, result);
			return result;
		};

		concurrent::pool::Task initial_task(250, [&fib_task_gen](concurrent::worker::WRef) {
			return fib_task_gen(250);
		});

		concurrent::runOrTimeout(
			[&]() {
				task_pool.addInitialTasks({ std::move(initial_task) });
				task_pool.execute();
				task_pool.waitExecutionCompletion();
			},
			[&] { fail("Timeout"); }
		);

		std::cout << "Execution completed.\n";
	}

	void testFibbonaciQuery() {
		concurrent::pool::TaskPool task_pool;

		concurrent::ConHashMap<u64, u64> fib_cache;
		std::function<u64(u64)>          fib_task_gen;
		fib_task_gen = [&task_pool, &fib_cache, &fib_task_gen](u64 n) -> u64 {
			if (n <= 1) {
				fib_cache.put(n, n);
				return n;
			}

			concurrent::pool::Task task1(n - 1, [n, &fib_task_gen](concurrent::worker::WRef) {
				return fib_task_gen(n - 1);
			});

			concurrent::pool::Task task2(n - 2, [n, &fib_task_gen](concurrent::worker::WRef) {
				return fib_task_gen(n - 2);
			});

			task_pool.query(task1);
			task_pool.query(task2);

			auto value1 = fib_cache.getCopy(n - 1);
			auto value2 = fib_cache.getCopy(n - 2);

			u64 result = (value1 + value2) % static_cast<u64>(1e9 + 7);
			fib_cache.put(n, result);
			return result;
		};

		concurrent::pool::Task initial_task(250, [&fib_task_gen](concurrent::worker::WRef) {
			return fib_task_gen(250);
		});

		concurrent::runOrTimeout(
			[&]() {
				task_pool.addInitialTasks({ std::move(initial_task) });
				task_pool.execute();
				task_pool.waitExecutionCompletion();
			},
			[&] { fail("Timeout"); }
		);

		std::cout << "Execution completed.\n";
	}

	void testFibbonaciScheduleAndQuery() {
		concurrent::pool::TaskPool task_pool;

		concurrent::ConHashMap<u64, u64> fib_cache;
		std::function<u64(u64)>          fib_task_gen;
		fib_task_gen = [&task_pool, &fib_cache, &fib_task_gen](u64 n) -> u64 {
			if (n <= 1) {
				fib_cache.put(n, n);
				return n;
			}

			concurrent::pool::Task task1(n - 1, [n, &fib_task_gen](concurrent::worker::WRef) {
				return fib_task_gen(n - 1);
			});

			concurrent::pool::Task task2(n - 2, [n, &fib_task_gen](concurrent::worker::WRef) {
				return fib_task_gen(n - 2);
			});

			auto future1 = task_pool.schedule(std::move(task1));
			task_pool.query(task2);
			future1.await();

			auto value1 = fib_cache.getCopy(n - 1);
			auto value2 = fib_cache.getCopy(n - 2);

			u64 result = (value1 + value2) % static_cast<u64>(1e9 + 7);
			fib_cache.put(n, result);
			return result;
		};

		concurrent::pool::Task initial_task(250, [&fib_task_gen](concurrent::worker::WRef) {
			return fib_task_gen(250);
		});

		concurrent::runOrTimeout(
			[&]() {
				task_pool.addInitialTasks({ std::move(initial_task) });
				task_pool.execute();
				task_pool.waitExecutionCompletion();
			},
			[&] { fail("Timeout"); }
		);

		std::cout << "Execution completed.\n";
	}

	void testGibbonaci() {
		concurrent::pool::TaskPool task_pool;

		concurrent::ConHashMap<u64, u64> gib_cache;
		std::function<u64(u64)>          gib_task_gen;
		gib_task_gen = [&task_pool, &gib_cache, &gib_task_gen](u64 n) -> u64 {
			if (n <= 3) {
				gib_cache.put(n, n);
				return n;
			}

			concurrent::pool::Task task1(n - 1, [n, &gib_task_gen](concurrent::worker::WRef) {
				return gib_task_gen(n - 1);
			});

			concurrent::pool::Task task2(n - 2, [n, &gib_task_gen](concurrent::worker::WRef) {
				return gib_task_gen(n - 2);
			});

			concurrent::pool::Task task3(n - 3, [n, &gib_task_gen](concurrent::worker::WRef) {
				return gib_task_gen(n - 3);
			});

			concurrent::pool::Task task4(n - 4, [n, &gib_task_gen](concurrent::worker::WRef) {
				return gib_task_gen(n - 4);
			});

			auto future1 = task_pool.schedule(std::move(task1));
			auto future2 = task_pool.schedule(std::move(task2));
			auto future3 = task_pool.schedule(std::move(task3));
			task_pool.query(task4);
			future1.await();
			future2.await();
			future3.await();

			auto value1 = gib_cache.getCopy(n - 1);
			auto value2 = gib_cache.getCopy(n - 2);
			auto value3 = gib_cache.getCopy(n - 3);
			auto value4 = gib_cache.getCopy(n - 4);

			u64 result = (value1 + value2 + value3 + value4) % static_cast<u64>(1e9 + 7);
			gib_cache.put(n, result);
			return result;
		};

		concurrent::pool::Task initial_task(250, [&gib_task_gen](concurrent::worker::WRef) {
			return gib_task_gen(250);
		});

		concurrent::runOrTimeout(
			[&]() {
				task_pool.addInitialTasks({ std::move(initial_task) });
				task_pool.execute();
				task_pool.waitExecutionCompletion();
			},
			[&] { fail("Timeout"); }
		);

		std::cout << "Execution completed.\n";
	}
};

TESTER_COMMON_MAIN("/src/common/concurrent/tests/");
