#include <concurrent/base/collections/hash_map.hpp>
#include <concurrent/module_flags/worker_count.hpp>
#include <concurrent/worker/worker_manager.hpp>

#include <tester/tester.hpp>

#include <stdexcept>
#include <thread>

template<class F>
void runOrTimeout(F func, usize timeout_ms = 5'000) {
	std::atomic<bool> finished = false;

	std::jthread worker_thread([&func, &finished]() {
		func();
		finished.store(true, std::memory_order_relaxed);
	});

	auto start = std::chrono::steady_clock::now();
	while (!finished.load(std::memory_order_relaxed)) {
		auto now = std::chrono::steady_clock::now();
		auto elapsed_ms
			= std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
		if (elapsed_ms > timeout_ms)
			throw std::runtime_error("Test timed out after " + std::to_string(timeout_ms) + " ms");
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

using namespace concurrent::worker;

class WorkerManagerTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS WorkerManagerTest


public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		setWorkerCount(4);
		TESTER_ADD_TEST(basicFunctionalityTest);
		TESTER_ADD_TEST(taskPoolFibonacciTest);
	}

protected:
	void fail(std::string_view err, bool critical = true) override {
		try {
			runOrTimeout(WorkerManager::get().testPrivateAccessReloadState);
		} catch (const std::runtime_error& e) {
			message(base::strConcat(
				"WorkerManager reload state timed out during fail(). "
				"Possible deadlock detected. Error: ",
				e.what()
			));
		}
		tester::TestSuite::fail(err, critical);
	}

private:
	void basicFunctionalityTest() {
		std::atomic<usize> no_task_counter = 0;
		auto               now             = std::chrono::steady_clock::now();

		auto& worker_manager = WorkerManager::get();
		for (const auto& id: worker_manager.getAllWorkers()) {
			worker_manager.setNoTasksCallback(id, [&no_task_counter](WRef) {
				no_task_counter.fetch_add(1, std::memory_order_relaxed);
			});
		}
		ASSERT_TRUE(no_task_counter >= getWorkerCount());

		auto all_workers = worker_manager.getAllWorkers();
		ASSERT_EQUAL(all_workers.size(), getWorkerCount());

		auto free_workers = worker_manager.getFreeWorkers(getWorkerCount());
		ASSERT_EQUAL(free_workers.size(), getWorkerCount());

		std::atomic<usize> task_finished_counter = 0;
		constexpr usize    TASK_WAIT_TIME_MS     = 100;
		for (const auto& worker: worker_manager.getAllWorkers()) {
			worker->scheduleTask([&task_finished_counter, TASK_WAIT_TIME_MS](WRef) {
				std::this_thread::sleep_for(std::chrono::milliseconds(TASK_WAIT_TIME_MS));
				task_finished_counter.fetch_add(1, std::memory_order_relaxed);
			});
		}

		// Wait for all tasks to complete
		// We also expect that during this time the no_tasks_callback
		// has been called at least once per worker.
		runOrTimeout([&] {
			while (task_finished_counter.load(std::memory_order_relaxed) < getWorkerCount()
			       || no_task_counter.load(std::memory_order_relaxed) < getWorkerCount() * 2)
				std::cerr << "Waiting... Finished tasks: "
						  << task_finished_counter.load(std::memory_order_relaxed)
						  << ", No task callbacks: "
						  << no_task_counter.load(std::memory_order_relaxed) << "\n",
					std::this_thread::yield();
		});

		usize val = no_task_counter.load(std::memory_order_relaxed);
		std::cerr << "No task callback called " << val << " times.\n";
		// The no_tasks_callback should have been called at least once per worker,
		// but no more than **three** times per worker.
		ASSERT_TRUE(getWorkerCount() <= val && val <= getWorkerCount() * 3);

		auto elapsed    = std::chrono::steady_clock::now() - now;
		auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

		// Since we have N threads and N tasks that each take 100ms,
		// the total time should be just over 100ms.
		assertTrue(
			TASK_WAIT_TIME_MS <= elapsed_ms && elapsed_ms < TASK_WAIT_TIME_MS + 100,
			base::strConcat("Elapsed time: ", elapsed_ms, " ms")
		);

		ASSERT_EQUAL(task_finished_counter.load(std::memory_order_relaxed), getWorkerCount());
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

		std::queue<Task> tasks;
		std::mutex       task_mutex;

		std::atomic<usize>                total_completed_tasks = 0;
		concurrent::ConHashMap<u64, u64>  results;
		concurrent::ConHashMap<WRef, u64> worker_task_counts;

		const usize task_count      = 50'000;
		const usize start           = 10'000;
		const usize task_batch_size = 100;
		// Creates `task_count` tasks to compute Fibonacci numbers concurrently, ranged
		// from [start, start + task_count] (modulo MOD).
		// Complexity is hard to estimate here, but each task should take a few milliseconds.
		for (u64 i = 0; i < task_count; i++)
			tasks.emplace(
				[&fib, i, &results, &worker_task_counts, &total_completed_tasks](WRef worker) {
					u64 result = fib(start + i);

					// Update the total completed tasks
					total_completed_tasks.fetch_add(1, std::memory_order_relaxed);

					// Store the result
					results.put(i, result);

					// Update the task count for this worker
					worker_task_counts.maybePutAndUpdate(worker, 0ULL, [](u64& count_ref) {
						count_ref++;
					});
				}
			);

		auto& worker_manager = WorkerManager::get();

		for (const auto& id: worker_manager.getAllWorkers()) {
			worker_manager.setNoTasksCallback(id, [&tasks, &task_mutex](WRef worker) {
				// This callback is invoked when a worker has no tasks.
				// We can use it to assign new tasks to the worker.
				std::scoped_lock lock(task_mutex);

				for (usize i = 0; i < task_batch_size; i++) {
					if (!tasks.empty()) {
						auto task = tasks.front();
						tasks.pop();
						worker->scheduleTask(task);
					} else {
						break;
					}
				}
			});
		}


		runOrTimeout([&] {
			while (total_completed_tasks.load(std::memory_order_relaxed) < task_count) {
				std::cerr << "Completed " << total_completed_tasks.load(std::memory_order_relaxed)
						  << " / " << task_count << " tasks.\n";
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		});
		std::cerr << "Completed " << total_completed_tasks.load(std::memory_order_relaxed) << " / "
				  << task_count << " tasks.\n";

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
