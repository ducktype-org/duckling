#pragma once

#include <base/extend_cpp/strongly_typed_id.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <concepts>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <thread>
#include <type_traits>

namespace concurrent::worker {
	class Worker;
	using WRef = Ref<Worker>;

	/**
	 * @brief Move-only type-erased callable used for worker jobs and callbacks.
	 *
	 * Stores any callable invocable as `fn(WRef)` by keeping:
	 * - a pointer to the concrete callable object,
	 * - a function pointer that invokes it,
	 * - a function pointer that destroys it.
	 *
	 * This gives one lightweight, uniform task type without requiring inheritance,
	 * while still supporting move-only callables.
	 */
	class Work final {
	public:
		Work() = default;

		template<typename F>
		requires(std::invocable<std::decay_t<F>&, WRef> && !std::same_as<std::decay_t<F>, Work>)
		Work(F&& fn):
			  callable_data(::new std::decay_t<F>(std::forward<F>(fn))),
			  invoke_fn(&invokeCallable<std::decay_t<F>>),
			  destroy_fn(&destroyCallable<std::decay_t<F>>) {}

		Work(const Work&)            = delete;
		Work& operator=(const Work&) = delete;

		Work(Work&& other) noexcept:
			  callable_data(std::exchange(other.callable_data, nullptr)),
			  invoke_fn(std::exchange(other.invoke_fn, nullptr)),
			  destroy_fn(std::exchange(other.destroy_fn, nullptr)) {}

		Work& operator=(Work&& other) noexcept {
			if (this == &other) return *this;

			if (callable_data != nullptr) destroy_fn(callable_data);

			callable_data = std::exchange(other.callable_data, nullptr);
			invoke_fn     = std::exchange(other.invoke_fn, nullptr);
			destroy_fn    = std::exchange(other.destroy_fn, nullptr);
			return *this;
		}

		~Work() {
			if (callable_data != nullptr) destroy_fn(callable_data);
		}

		void operator()(WRef worker_ref) const {
			if (invoke_fn == nullptr) return;
			invoke_fn(callable_data, worker_ref);
		}

	private:
		template<typename Callable>
		static void invokeCallable(void* callable, WRef worker_ref) {
			auto* callable_internal = static_cast<Callable*>(callable);
			(*callable_internal)(worker_ref);
		}

		template<typename Callable>
		static void destroyCallable(void* callable) {
			::delete static_cast<Callable*>(callable);
		}

		/// Type-erased storage for the concrete callable instance.
		/// Needed because Work must hold many callable types behind one uniform interface.
		void* callable_data = nullptr;

		/// Type-erased invoker for the object stored in callable_data.
		/// Needed to call the original callable without virtual inheritance or std::function copies.
		void (*invoke_fn)(void*, WRef) = nullptr;

		/// Type-erased destructor for the object stored in callable_data.
		/// Needed to destroy the correct concrete callable type during move/reset/destruction.
		void (*destroy_fn)(void*)      = nullptr;
	};

	/**
	 * @brief Callback type invoked when a worker has no tasks to execute.
	 */
	using NoTasksCallback = Work;

	/**
	 * @brief Represents a task to be executed by a worker.
	 */
	using Task = Work;

	/**
	 * @brief Represents a single worker in the WorkerManager.
	 * @note All methods are thread-safe.
	 * Worker's inner thread is joined upon destruction.
	 */
	class Worker final {
		friend class WorkerManager;

	public:
		Worker()                         = delete;
		Worker(const Worker&)            = delete;
		Worker(Worker&&)                 = delete;
		Worker& operator=(const Worker&) = delete;
		Worker& operator=(Worker&&)      = delete;

		~Worker();

		/**
		 * @brief Pushes a task to the worker's task queue.
		 * @param task The task to be executed.
		 */
		void scheduleTask(Task&& task);

		/**
		 * @brief Pushes a task to the worker's task queue via move only if the worker is free.
		 * @param task The task to be executed.
		 * @return True if the task was pushed, false otherwise.
		 */
		bool scheduleTaskIfFree(Task&& task);

		/**
		 * @brief Checks if a worker is free.
		 * Free means that the worker is not currently executing any task
		 * and has no tasks in its queue.
		 */
		[[nodiscard]] bool isFree() const;

		/**
		 * @brief Checks if the worker has tasks in its queue.
		 * Used specifically when you need it and when you are sure
		 * that there are no race conditions on scheduling the tasks.
		 *
		 * Used in the TaskPool to prevent the situation when we exit
		 * the worker's no_tasks_callback without new schedules
		 * and before setting the worker as free we want to schedule something.
		 */
		[[nodiscard]] bool internalHasTasks() const;

		/**
		 * @brief Sets the callback to be invoked when there are no tasks.
		 * @param callback The callback function.
		 * @note If a worker is free, it will call the new callback
		 * immediately. This means, that the callback may be called more than once
		 * if the worker has no tasks - once in the method call, and later when the worker
		 * loop checks for tasks. If the first call adds tasks, then the second call will not
		 * happen.
		 * @warning The lifetime of the variables used in the callback must be guaranteed to be
		 * longer than the lifetime of a callback inside a worker. This means that you need to make
		 * sure to reset the callback before the destruction of the variables
		 */
		void setNoTasksCallback(NoTasksCallback&& callback);

		/**
		 * @brief Gets the reference of the current worker
		 * Panics if called from a non-worker thread.
		 */
		static WRef getCurrentWorker();

		static bool isCurrentThreadWorker();

		/**
		 * @brief Gets the unique u64 ID of the worker.
		 */
		[[nodiscard]]
		u64 getID() const;

	private:
		Worker(usize seed);

		/**
		 * @brief Starts the worker's main loop in a separate thread.
		 */
		void run();

		/**
		 * @brief Sets the callback to be invoked when there are no tasks.
		 * @param callback Shared callback instance.
		 */
		void setNoTasksCallback(std::shared_ptr<NoTasksCallback> callback);

		/**
		 * @brief Generates a random u64 using the worker's RNG.
		 * @note This is NOT thread-safe, as std::mt19937_64 is not thread-safe. It should only be
		 * called from the worker's main loop or with external synchronization. The randomness is
		 * not guaranteed to be high-quality, but it is sufficient for load balancing tasks.
		 */
		u64 randomU64() const { return u64(rng()); }

		/**
		 * @brief The random number generator for the worker.
		 */
		mutable std::mt19937_64 rng;

		/**
		 * @brief Indicates whether the worker is free.
		 */
		std::atomic_bool is_free       = true;
		std::atomic_bool loop_run_flag = true;  /// Controls the main loop of the worker thread.

		std::shared_ptr<NoTasksCallback> no_tasks_callback /// Callback when there are no tasks.
			= std::make_shared<NoTasksCallback>([](WRef) {});  /// Shared with worker loop and setter so the callback can be replaced safely.

		std::queue<Task> task_queue;

		mutable std::mutex mut;  /// Internal synchronization mutex.
		std::condition_variable
			task_cv;             /// Condition variable to notify the worker thread about new tasks.

		std::jthread real_thread;

		static constinit u64 next_id;
		u64                         id      = next_id++;  /// Unique u64 ID for the worker
	};
}

template<>
struct std::hash<concurrent::worker::WRef> {
	[[nodiscard]] size_t operator()(const concurrent::worker::WRef& worker_ref) const noexcept {
		return std::hash<usize>{}(reinterpret_cast<usize>(worker_ref.get()));
	}
};
