#pragma once

#include <concurrent/thread_manager/task.hpp>

#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/types/ints.hpp>

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

namespace concurrent {
	STRONG_TYPEDEF_ID_DIRECT_CREATION(ThreadID);

	/**
	 * @brief Represents a single thread in the ThreadManager.
	 * @note All methods are thread-safe.
	 */
	class Thread {
	public:
		Thread(ThreadID id);
		~Thread();

		Thread(const Thread&)            = delete;
		Thread(Thread&&)                 = delete;
		Thread& operator=(const Thread&) = delete;
		Thread& operator=(Thread&&)      = delete;

		void pushTask(Task&& task);

		/**
		 * @brief Checks if a thread is free.
		 * Free means that the thread is not currently executing any task
		 * and has no tasks in its queue.
		 */
		[[nodiscard]] bool isFree() const;

		void stop() {
			{
				std::lock_guard<std::mutex> lock(m);
				loop_run_flag = false;
			}
			cv.notify_one();
			worker_thread.join();
		}

		[[nodiscard]] ThreadID getId() const;

	private:
		std::atomic_bool is_occupied   = false;
		std::atomic_bool loop_run_flag = true;

		const ThreadID id;

		std::queue<Task> task_queue;

		mutable std::mutex      m;
		std::condition_variable cv;

		std::thread worker_thread;
	};
}
