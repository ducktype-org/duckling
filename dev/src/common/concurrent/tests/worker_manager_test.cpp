#include <concurrent/base/collections/hash_map.hpp>
#include <concurrent/base/run_or_timeout.hpp>
#include <concurrent/module_flags/worker_count.hpp>
#include <concurrent/worker/worker_manager.hpp>

#include <tester/tester.hpp>

#include <iostream>
#include <mutex>
#include <thread>


using namespace concurrent::worker;

class WorkerManagerTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS WorkerManagerTest


public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(basicFunctionalityTest);
		TESTER_ADD_TEST(taskPoolFibonacciTest);
	}

protected:
	void beforeAll() override { setWorkerCount(4); }

	void fail(std::string_view err, bool critical = true) override {
		concurrent::runOrTimeout(WorkerManager::get().testPrivateAccessReloadState, [&] {
			message(
				base::strConcat("WorkerManager reload state timed out during fail(). "
			                    "Terminating.")
			);
		});
		tester::TestSuite::fail(err, critical);
	}

private:
	void basicFunctionalityTest() {
		std::atomic<usize> no_task_counter = 0;
		auto               now             = std::chrono::steady_clock::now();

		auto& worker_manager = WorkerManager::get();

		auto all_workers = worker_manager.getAllWorkers();
		ASSERT_EQUAL(all_workers.size(), getWorkerCount());

		auto free_workers = worker_manager.getFreeWorkers(getWorkerCount());
		ASSERT_EQUAL(free_workers.size(), getWorkerCount());

		for (const auto& id: worker_manager.getAllWorkers()) {
			worker_manager.setNoTasksCallback(id, [&no_task_counter](WRef) {
				no_task_counter.fetch_add(1, std::memory_order_relaxed);
			});
		}

		std::atomic<usize> task_finished_counter = 0;
		constexpr usize    TASK_WAIT_TIME_MS     = 100;
		for (const auto& worker: worker_manager.getAllWorkers()) {
			worker->scheduleTask([&task_finished_counter, TASK_WAIT_TIME_MS](WRef wref) {
				std::this_thread::sleep_for(std::chrono::milliseconds(TASK_WAIT_TIME_MS));
				task_finished_counter.fetch_add(1, std::memory_order_relaxed);
				wref->setNoTasksCallback([](WRef) {});
			});
		}

		// Wait for all tasks to complete
		// We also expect that during this time the no_tasks_callback
		// has been called at least once per worker.
		concurrent::runOrTimeout(
			[&](const std::stop_token& st) {
				while (!st.stop_requested()
			           && (task_finished_counter.load(std::memory_order_relaxed) < getWorkerCount())
			    ) {
					std::cerr << "Waiting... Finished tasks: "
							  << task_finished_counter.load(std::memory_order_relaxed)
							  << ", No task callbacks: "
							  << no_task_counter.load(std::memory_order_relaxed) << "\n";
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}
			},
			[&] { fail("Timeout"); }
		);

		usize val = no_task_counter.load(std::memory_order_relaxed);
		std::cerr << "No task callback called " << val << " times.\n";
		// The no_tasks_callback should have been called at least once per worker,
		// but no more than **two** times per worker.
		ASSERT_TRUE(getWorkerCount() <= val && val <= getWorkerCount() * 2);

		auto elapsed    = std::chrono::steady_clock::now() - now;
		auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

		// Since we have N threads and N tasks that each take 100ms,
		// the total time should be just over 100ms.
		assertTrue(
			TASK_WAIT_TIME_MS <= elapsed_ms && elapsed_ms < TASK_WAIT_TIME_MS + 100,
			base::strConcat("Elapsed time: ", elapsed_ms, " ms")
		);

		ASSERT_EQUAL(task_finished_counter.load(std::memory_order_relaxed), getWorkerCount());

		// After all tasks are done, all workers should be free.
		concurrent::runOrTimeout(
			[&](const std::stop_token&) {
				worker_manager.waitForAllWorkersFree(std::chrono::milliseconds(10));
			},
			[&] { fail("Timeout while waiting for all workers to be free"); }
		);

		free_workers = worker_manager.getFreeWorkers(getWorkerCount());
		ASSERT_EQUAL(free_workers.size(), getWorkerCount());
	}

	void taskPoolFibonacciTest() {
		constexpr u64 MOD = static_cast<u64>(1e9 + 7);

		// A simple Fibonacci function. It is a "CPU-bound" task.
		const auto fib = [](u64 n) -> u64 {
			u64 a = 0;
			u64 b = 1;
			for (u64 i = 0; i < n; i++) {
				u64 next = (a + b) % MOD;
				a        = b;
				b        = next;
			}
			return a;
		};


		std::mutex                        task_mutex;
		std::atomic<u64>                  total_completed_tasks = 0;
		std::queue<Task>                  tasks;
		concurrent::ConHashMap<u64, u64>  results;
		concurrent::ConHashMap<WRef, u64> worker_task_counts;

		constexpr u64 TASK_COUNT      = 10'000;
		constexpr u64 START           = 10'000;
		constexpr u64 TASK_BATCH_SIZE = 5;
		// Creates `task_count` tasks to compute Fibonacci numbers concurrently, ranged
		// from [start, start + task_count] (modulo MOD).
		// Complexity is hard to estimate here, but each task should take a few milliseconds.
		for (u64 i = 0; i < TASK_COUNT; i++)
			tasks.emplace(
				[&fib, i, &results, &worker_task_counts, &total_completed_tasks](WRef worker) {
					u64 result = fib(START + i);

					// Update the total completed tasks
					total_completed_tasks.fetch_add(1, std::memory_order_relaxed);

					// Store the result
					results.put(i, result);

					// Update the task count for this worker
					worker_task_counts.maybePutAndUpdate(worker, 0ULL, [](Ref<u64> count_ref) {
						*count_ref += 1;
					});
				}
			);

		auto& worker_manager = WorkerManager::get();

		for (const auto& id: worker_manager.getAllWorkers()) {
			worker_manager.setNoTasksCallback(id, [&tasks, &task_mutex](WRef worker) {
				// This callback is invoked when a worker has no tasks.
				// We can use it to assign new tasks to the worker.
				std::scoped_lock lock(task_mutex);

				for (usize i = 0; i < TASK_BATCH_SIZE; i++) {
					if (tasks.empty()) {
						break;
					} else {
						worker->scheduleTask(std::move(tasks.front()));
						tasks.pop();
					}
				}
			});
		}


		concurrent::runOrTimeout(
			[&](const std::stop_token& st) {
				while (!st.stop_requested()
			           && total_completed_tasks.load(std::memory_order_relaxed) < TASK_COUNT) {
					std::cerr << "Completed(inner) "
							  << total_completed_tasks.load(std::memory_order_relaxed) << " / "
							  << TASK_COUNT << " tasks.\n";
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
			},
			[&] {
				std::cerr << "Leaving out of testing...\n";
				std::cerr << "Completed " << total_completed_tasks.load(std::memory_order_relaxed)
						  << " / " << TASK_COUNT << " tasks.\n";

				// Print worker task counts
				for (const auto& worker: worker_manager.getAllWorkers()) {
					u64 count = worker_task_counts.getCopy(worker);
					std::cerr << "Worker " << usize(worker.get()) << " completed " << count
							  << " tasks.\n";
				}
				fail("Timeout.");
			}
		);

		std::scoped_lock lock(task_mutex);
		std::cerr << "Completed(outer) " << total_completed_tasks.load(std::memory_order_relaxed)
				  << " / " << TASK_COUNT << " tasks.\n";

		// Print worker task counts
		for (const auto& worker: worker_manager.getAllWorkers()) {
			u64 count = worker_task_counts.getCopy(worker);
			std::cerr << "Worker " << usize(worker.get()) << " completed " << count << " tasks.\n";
		}

		// Ensure that each worker has completed at least one task
		for (const auto& worker: worker_manager.getAllWorkers()) {
			u64 count = worker_task_counts.getCopy(worker);
			ASSERT_TRUE(count > 0);
		}

		// Validate the results
		u64 a = fib(START);
		u64 b = fib(START + 1);
		for (u64 i = 0; i < TASK_COUNT; i++) {
			assertEqual(
				results.getCopy(i),
				a,
				base::strConcat(
					"Incorrect result for task ", i, " expected", a, " got ", results.getCopy(i)
				)
			);
			u64 next = (a + b) % MOD;
			a        = b;
			b        = next;
		}

		// After all tasks are done, all workers should be free.
		concurrent::runOrTimeout(
			[&](const std::stop_token&) {
				worker_manager.waitForAllWorkersFree(std::chrono::milliseconds(10));
			},
			[&] { fail("Timeout while waiting for all workers to be free"); }
		);
	}
};

TESTER_COMMON_MAIN("/src/common/concurrent/tests/");
