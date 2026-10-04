// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <concurrent/base/collections/hash_map.hpp>
#include <concurrent/base/run_or_timeout.hpp>
#include <concurrent/module_flags/worker_count.hpp>
#include <concurrent/worker/worker.hpp>
#include <concurrent/worker/worker_manager.hpp>

#include <query_framework/internal/task_pool/task_pool.hpp>
#include <tester/tester.hpp>

class TaskPoolTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS TaskPoolTest


public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(basicFunctionalityTest);
		TESTER_ADD_TEST(testFibonacciSchedule);
		TESTER_ADD_TEST(testFibonacciScheduleReversed);
		TESTER_ADD_TEST(testFibonacciQuery);
		TESTER_ADD_TEST(testFibonacciScheduleAndQuery);
		TESTER_ADD_TEST(testGibonacci);
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
	static bool fakeEraseFunction(query::QueryStableHash) { return false; }

	static query::internal::QueryID getFakeQueryID() {
		static const query::internal::QueryID fake_query_id
			= query::internal::registerQuery(query::internal::QueryData(
				query::internal::QueryKind::Normal,
				"TaskPoolTestFakeQuery",
				query::internal::QueryTags{ .used_hashes  = query::UsedHashes::UnstableHash,
		                                    .uses_qresult = false },
				query::internal::QueryCacheData{ .erase_function = &fakeEraseFunction }
			));
		return fake_query_id;
	}

	static query::internal::NodeID makeFakeNodeID(u64 value) {
		return { getFakeQueryID(), query::internal::KeyHash{ base::Bit256(value) } };
	}

	void basicFunctionalityTest() {
		query::internal::TaskPool task_pool;
		constexpr usize           TASK_COUNT      = 10;
		std::atomic_int           completed_tasks = 0;

		for (usize i = 0; i < TASK_COUNT; ++i) {
			auto node_id = makeFakeNodeID(i + 1);
			task_pool.addTask(query::internal::Task(
				node_id,
				[&completed_tasks](concurrent::worker::WRef) {
					completed_tasks.fetch_add(1, std::memory_order_relaxed);
				}
			));
		}

		for (usize i = 0; i < TASK_COUNT; ++i) task_pool.waitForTask(makeFakeNodeID(i + 1));

		std::cout << "Execution completed.\n";
		ASSERT_EQUAL(completed_tasks.load(std::memory_order_relaxed), TASK_COUNT);
	}

	void testFibonacciSchedule() {
		query::internal::TaskPool task_pool;

		concurrent::ConHashMap<u64, u64> fib_cache;
		std::function<u64(u64)>          fib_task_gen;
		fib_task_gen = [&task_pool, &fib_cache, &fib_task_gen](u64 n) -> u64 {
			if (n <= 1) {
				fib_cache.put(n, n);
				return n;
			}

			query::internal::Task task1(
				makeFakeNodeID(n - 1),
				[n, &fib_task_gen](concurrent::worker::WRef) { return fib_task_gen(n - 1); }
			);

			query::internal::Task task2(
				makeFakeNodeID(n - 2),
				[n, &fib_task_gen](concurrent::worker::WRef) { return fib_task_gen(n - 2); }
			);

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

		auto root_id = makeFakeNodeID(250);

		concurrent::runOrTimeout(
			[&]() {
				task_pool.addTask(query::internal::Task(
					root_id, [&fib_task_gen](concurrent::worker::WRef) { return fib_task_gen(250); }
				));
				task_pool.waitForTask(root_id);
			},
			[&] { fail("Timeout"); }
		);
		std::cout << "Execution completed.\n";
	}

	void testFibonacciScheduleReversed() {
		query::internal::TaskPool task_pool;

		concurrent::ConHashMap<u64, u64> fib_cache;
		std::function<u64(u64)>          fib_task_gen;
		fib_task_gen = [&task_pool, &fib_cache, &fib_task_gen](u64 n) -> u64 {
			if (n <= 1) {
				fib_cache.put(n, n);
				return n;
			}

			query::internal::Task task1(
				makeFakeNodeID(n - 1),
				[n, &fib_task_gen](concurrent::worker::WRef) { return fib_task_gen(n - 1); }
			);

			query::internal::Task task2(
				makeFakeNodeID(n - 2),
				[n, &fib_task_gen](concurrent::worker::WRef) { return fib_task_gen(n - 2); }
			);

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

		auto root_id = makeFakeNodeID(250);

		concurrent::runOrTimeout(
			[&]() {
				task_pool.addTask(query::internal::Task(
					root_id, [&fib_task_gen](concurrent::worker::WRef) { return fib_task_gen(250); }
				));
				task_pool.waitForTask(root_id);
			},
			[&] { fail("Timeout"); }
		);

		std::cout << "Execution completed.\n";
	}

	void testFibonacciQuery() {
		query::internal::TaskPool task_pool;

		concurrent::ConHashMap<u64, u64> fib_cache;
		std::function<u64(u64)>          fib_task_gen;
		fib_task_gen = [&task_pool, &fib_cache, &fib_task_gen](u64 n) -> u64 {
			if (n <= 1) {
				fib_cache.put(n, n);
				return n;
			}

			query::internal::Task task1(
				makeFakeNodeID(n - 1),
				[n, &fib_task_gen](concurrent::worker::WRef) { return fib_task_gen(n - 1); }
			);

			query::internal::Task task2(
				makeFakeNodeID(n - 2),
				[n, &fib_task_gen](concurrent::worker::WRef) { return fib_task_gen(n - 2); }
			);

			task_pool.query(task1);
			task_pool.query(task2);

			auto value1 = fib_cache.getCopy(n - 1);
			auto value2 = fib_cache.getCopy(n - 2);

			u64 result = (value1 + value2) % static_cast<u64>(1e9 + 7);
			fib_cache.put(n, result);
			return result;
		};

		auto root_id = makeFakeNodeID(250);

		concurrent::runOrTimeout(
			[&]() {
				task_pool.addTask(query::internal::Task(
					root_id, [&fib_task_gen](concurrent::worker::WRef) { return fib_task_gen(250); }
				));
				task_pool.waitForTask(root_id);
			},
			[&] { fail("Timeout"); }
		);

		std::cout << "Execution completed.\n";
	}

	void testFibonacciScheduleAndQuery() {
		query::internal::TaskPool task_pool;

		concurrent::ConHashMap<u64, u64> fib_cache;
		std::function<u64(u64)>          fib_task_gen;
		fib_task_gen = [&task_pool, &fib_cache, &fib_task_gen](u64 n) -> u64 {
			if (n <= 1) {
				fib_cache.put(n, n);
				return n;
			}

			query::internal::Task task1(
				makeFakeNodeID(n - 1),
				[n, &fib_task_gen](concurrent::worker::WRef) { return fib_task_gen(n - 1); }
			);

			query::internal::Task task2(
				makeFakeNodeID(n - 2),
				[n, &fib_task_gen](concurrent::worker::WRef) { return fib_task_gen(n - 2); }
			);

			auto future1 = task_pool.schedule(std::move(task1));
			task_pool.query(task2);
			future1.await();

			auto value1 = fib_cache.getCopy(n - 1);
			auto value2 = fib_cache.getCopy(n - 2);

			u64 result = (value1 + value2) % static_cast<u64>(1e9 + 7);
			fib_cache.put(n, result);
			return result;
		};

		auto root_id = makeFakeNodeID(250);

		concurrent::runOrTimeout(
			[&]() {
				task_pool.addTask(query::internal::Task(
					root_id, [&fib_task_gen](concurrent::worker::WRef) { return fib_task_gen(250); }
				));
				task_pool.waitForTask(root_id);
			},
			[&] { fail("Timeout"); }
		);

		std::cout << "Execution completed.\n";
	}

	void testGibonacci() {
		query::internal::TaskPool task_pool;

		concurrent::ConHashMap<u64, u64> gib_cache;
		std::function<u64(u64)>          gib_task_gen;
		gib_task_gen = [&task_pool, &gib_cache, &gib_task_gen](u64 n) -> u64 {
			if (n <= 3) {
				gib_cache.put(n, n);
				return n;
			}

			query::internal::Task task1(
				makeFakeNodeID(n - 1),
				[n, &gib_task_gen](concurrent::worker::WRef) { return gib_task_gen(n - 1); }
			);

			query::internal::Task task2(
				makeFakeNodeID(n - 2),
				[n, &gib_task_gen](concurrent::worker::WRef) { return gib_task_gen(n - 2); }
			);

			query::internal::Task task3(
				makeFakeNodeID(n - 3),
				[n, &gib_task_gen](concurrent::worker::WRef) { return gib_task_gen(n - 3); }
			);

			query::internal::Task task4(
				makeFakeNodeID(n - 4),
				[n, &gib_task_gen](concurrent::worker::WRef) { return gib_task_gen(n - 4); }
			);

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

		auto root_id = makeFakeNodeID(250);

		concurrent::runOrTimeout(
			[&]() {
				task_pool.addTask(query::internal::Task(
					root_id, [&gib_task_gen](concurrent::worker::WRef) { return gib_task_gen(250); }
				));
				task_pool.waitForTask(root_id);
			},
			[&] { fail("Timeout"); }
		);

		std::cout << "Execution completed.\n";
	}
};

TESTER_COMMON_MAIN("/src/common/query_framework/tests/");
