#include <concurrent/base/collections/hash_map.hpp>
#include <concurrent/module_flags/worker_count.hpp>
#include <concurrent/worker/worker_data.hpp>
#include <concurrent/worker/worker_manager.hpp>

#include <tester/tester.hpp>

#include <thread>

class ThreadManagerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS ThreadManagerTest


public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		concurrent::setWorkerCount(4);
		TESTER_ADD_TEST(basicFunctionalityTest);
		TESTER_ADD_TEST(taskPoolFibonacciTest);
	}

private:
	void basicFunctionalityTest() {
		std::atomic<usize> no_task_counter = 0;
		auto               now             = std::chrono::steady_clock::now();

		concurrent::worker::WorkerManager worker_manager;

		for (const auto& id: worker_manager.getAllWorkers()) {
			worker_manager.setNoTasksCallback(id, [&no_task_counter](concurrent::worker::WDRef) {
				no_task_counter.fetch_add(1, std::memory_order_relaxed);
			});
		}

		auto all_workers = worker_manager.getAllWorkers();
		ASSERT_EQUAL(all_workers.size(), concurrent::getWorkerCount());

		auto free_workers = worker_manager.getFreeWorkers(concurrent::getWorkerCount());
		ASSERT_EQUAL(free_workers.size(), concurrent::getWorkerCount());

		std::atomic<usize> task_finished_counter = 0;
		for (const auto& id: worker_manager.getAllWorkers()) {
			worker_manager.scheduleTaskOnWorker(
				id,
				[&task_finished_counter](concurrent::worker::WDRef) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1'000));
					task_finished_counter.fetch_add(1, std::memory_order_relaxed);
				}
			);
		}

		// Wait for all tasks to complete
		while (task_finished_counter.load(std::memory_order_relaxed) < concurrent::getWorkerCount())
			std::this_thread::yield();

		// The no_tasks_callback should have been called at least once per worker,
		// but no more than twice per worker (The tasks may have been scheduled before the worker
		// has initialized).
		ASSERT_TRUE(
			concurrent::getWorkerCount() <= no_task_counter
			&& no_task_counter <= concurrent::getWorkerCount() * 2
		);

		auto elapsed    = std::chrono::steady_clock::now() - now;
		auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

		// Since we have N threads and N tasks that each take 1 second,
		// the total time should be just over 1 second (1000 ms).
		assertTrue(elapsed_ms < 1'100, base::strConcat("Elapsed time: ", elapsed_ms, " ms"));

		ASSERT_EQUAL(
			task_finished_counter.load(std::memory_order_relaxed), concurrent::getWorkerCount()
		);
	}

	void taskPoolFibonacciTest() {
		constexpr u64 MOD = static_cast<u64>(1e9 + 7);

		// A simple Fibonacci function. It is a "CPU-bound" task.
		const auto fib = [](u64 n) -> u64 {
			usize a = 0;
			usize b = 1;
			for (u64 i = 0; i < n; i++) {
				usize next = (a + b) % MOD;
				a          = b;
				b          = next;
			}
			return a;
		};

		std::queue<concurrent::worker::Task> tasks;
		std::mutex                           task_mutex;

		std::atomic<usize>               total_completed_tasks = 0;
		concurrent::ConHashMap<u64, u64> results;
		concurrent::ConHashMap<u64, u64> worker_task_counts;

		const usize task_count      = 100'000;
		const usize start           = 10'000;
		const usize task_batch_size = 100;
		// Creates `task_count` tasks to compute Fibonacci numbers concurrently, ranged
		// from [start, start + task_count] (modulo MOD).
		// Complexity is really hard to estimate here, but each task should take a few milliseconds.
		for (u64 i = 0; i < task_count; i++)
			tasks.emplace([&fib, i, &results, &worker_task_counts, &total_completed_tasks](
							  concurrent::worker::WDRef wd
						  ) {
				u64 result = fib(start + i);

				// Update the total completed tasks
				total_completed_tasks.fetch_add(1, std::memory_order_relaxed);

				// Store the result
				results.put(i, result);

				// Update the task count for this worker
				worker_task_counts.maybePutAndUpdate(
					static_cast<u64>(wd->getID()), 0ULL, [](u64& count_ref) { count_ref++; }
				);
			});

		concurrent::worker::WorkerManager worker_manager;

		for (auto& id: worker_manager.getAllWorkers()) {
			worker_manager.setNoTasksCallback(
				id,
				[&tasks, &task_mutex, &worker_manager](concurrent::worker::WDRef wd) {
					// This callback is invoked when a worker has no tasks.
				    // We can use it to assign new tasks to the worker.
					std::scoped_lock lock(task_mutex);

					for (usize i = 0; i < task_batch_size; i++) {
						if (!tasks.empty()) {
							auto task = tasks.front();
							tasks.pop();
							worker_manager.scheduleTaskOnWorker(wd->getID(), std::move(task));
						} else {
							break;
						}
					}
				}
			);
		}


		while (total_completed_tasks.load(std::memory_order_relaxed) < task_count) {
			std::cerr << "Completed " << total_completed_tasks.load(std::memory_order_relaxed)
					  << " / " << task_count << " tasks.\n";
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		// Print worker task counts
		for (const auto& worker_id: worker_manager.getAllWorkers()) {
			u64 count = worker_task_counts.getCopy(static_cast<usize>(worker_id));
			std::cerr << "Worker " << static_cast<usize>(worker_id) << " completed " << count
					  << " tasks.\n";
		}

		// Validate the results
		u64 a = fib(start);
		u64 b = fib(start + 1);
		for (u64 i = 0; i < task_count; i++) {
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
	}
};

TESTER_COMMON_MAIN("/src/common/concurrent/tests/");
