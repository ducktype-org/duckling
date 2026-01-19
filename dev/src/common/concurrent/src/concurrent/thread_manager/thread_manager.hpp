#pragma once

#include <concurrent/module_flags/worker_count.hpp>
#include <concurrent/thread_manager/thread.hpp>

#include <base/pointers/box.hpp>
#include <base/types/ints.hpp>

#include <ranges>
#include <vector>

namespace concurrent {

	/**
	 * @brief Manages a fixed number of threads to execute tasks.
	 * @note Using ThreadID between different ThreadManager instances is undefined.
	 */
	class ThreadManager {
	public:
		/**
		 * @brief Constructs a ThreadManager with the specified number of threads.
		 * @param num_threads The number of threads to manage. Defaults to the worker count.
		 */
		ThreadManager(usize num_threads = concurrent::getWorkerCount()):
			  num_threads(num_threads),
			  threads{ std::views::iota(usize{ 0 }, num_threads)
			           | std::views::transform([](usize i) {
							 return makeBox<Thread>(ThreadID{ i });
						 })
			           | std::ranges::to<std::vector<Box<Thread>>>() } {}

		/**
		 * @brief Returns a vector of all thread IDs managed by the ThreadManager.
		 */
		std::vector<ThreadID> getAllThreads() {
			return threads
			     | std::views::transform([](const Box<Thread>& thread) { return thread->getId(); })
			     | std::ranges::to<std::vector<ThreadID>>();
		}

		/**
		 * @brief Returns a vector of free thread IDs.
		 * @param max_count The maximum number of free thread IDs to return.
		 */
		std::vector<ThreadID> getFreeThreads(usize max_count) {
			return threads | std::ranges::views::filter([](const Box<Thread>& thread) {
					   return thread->isFree();
				   })
			     | std::ranges::views::take(max_count)
			     | std::ranges::views::transform([](const Box<Thread>& thread) {
					   return thread->getId();
				   })
			     | std::ranges::to<std::vector<ThreadID>>();
		}

		/**
		 * @brief Schedules a task on a specific thread.
		 * @param thread_id The ID of the thread to schedule the task on.
		 * @param task The task to be executed.
		 */
		void scheduleTaskOnThread(ThreadID thread_id, Task task) {
			threads[static_cast<usize>(thread_id)]->pushTask(std::move(task));
		}

		/**
		 * @brief Schedules a task on any free thread.
		 * If no free thread is available, the task is scheduled on a random thread.
		 * @param task The task to be executed.
		 */
		void scheduleTaskOnAnyFreeThread(Task&& task) {
			for (auto& thread: threads) {
				if (thread->isFree()) {
					thread->pushTask(std::move(task));
					return;
				}
			}
			// NOLINTBEGIN(concurrency-mt-unsafe)
			// If no free thread is found, push to a random thread
			threads[static_cast<usize>(std::rand()) % num_threads]->pushTask(std::move(task));
			// NOLINTEND(concurrency-mt-unsafe)
		}

		/**
		 * @brief Checks if a thread is free.
		 * @param thread_id The ID of the thread to check.
		 * @return True if the thread is free, false otherwise.
		 */
		[[nodiscard]] bool isThreadFree(ThreadID thread_id) const {
			return threads[static_cast<usize>(thread_id)]->isFree();
		}

	private:
		/**
		 * @brief The number of threads managed by the ThreadManager.
		 */
		const usize num_threads;

		/**
		 * @brief Array of threads managed by the ThreadManager.
		 */
		std::vector<Box<Thread>> threads;
	};

}
