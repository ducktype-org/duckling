#pragma once

#include <base/collections/optional.hpp>
#include <base/pointers/ref.hpp>
#include <base/types/ints.hpp>

#include <vm/api/data/thread_id.hpp>
#include <vm/core/process/process_state.hpp>
#include <vm/core/thread/thread_state.hpp>

#include <condition_variable>
#include <expected>
#include <functional>
#include <mutex>
#include <string>

namespace vm {

	/**
	 * @brief The only way to read or change the VMThread states of one VMProcess.
	 *
	 * The states themselves are stored in `ProcessStateAggregation`. This class handles
	 * the synchronization around it and derives the process state from the thread states, and it
	 * wakes the waiters and emits the status change. Threads do not own their state - they apply
	 * transitions here.
	 */
	class ProcessStateManager final {
	public:
		using ThreadState  = thread_state::ThreadState;
		using ThreadEvent  = thread_event::ThreadEvent;
		using ProcessState = process_state::ProcessState;

		/**
		 * @brief Callback called whenever the aggregate process state changes.
		 */
		using OnStatusChangedCallback = std::function<void(const ProcessState&)>;

		/**
		 * @brief Read-only view of the table handed to `wait` and the per-thread waits.
		 * Valid ONLY for the duration of the `wait` predicate call (it is a view under the lock).
		 */
		class Snapshot final {
		public:
			/// State of a given thread. Panics on an unknown `tid`.
			[[nodiscard]] const ThreadState& threadState(api::ThreadID tid) const;

			/// The aggregate process state.
			[[nodiscard]] ProcessState processState() const;

			/// Per-thread state change counter of a registered thread.
			/// Panics on an unknown `tid`.
			[[nodiscard]] u64 threadStateChangeCounter(api::ThreadID tid) const;

		private:
			friend class ProcessStateManager;

			explicit Snapshot(const ProcessStateManager& table): table(table) {}

			const ProcessStateManager& table;
		};

		struct ApplyResult {
			/**
			 * @brief true when this thread state change put the process into `Panicked` for the
			 * first time. The caller must send a Stop request out to all threads.
			 */
			bool stop_all_threads{ false };
		};

		explicit ProcessStateManager(api::ThreadID main_tid);

		ProcessStateManager(const ProcessStateManager&)            = delete;
		ProcessStateManager& operator=(const ProcessStateManager&) = delete;
		ProcessStateManager(ProcessStateManager&&)                 = delete;
		ProcessStateManager& operator=(ProcessStateManager&&)      = delete;

		/**
		 * @brief Sets the callback invoked on every process state change. Must be called
		 * once, before any thread is spawned.
		 */
		void setOnStatusChangedCallback(OnStatusChangedCallback callback);

		/**
		 * @brief Adds a thread to the table in the `NotStarted` state.
		 * Must be called before the thread's exec thread exists. Registering an already
		 * registered thread panics.
		 */
		void registerThread(api::ThreadID tid);

		/**
		 * @brief Only entry point to be called from the VMThread execution thread, never VMProcess.
		 * Given the @p `tid` and @p `event`:
		 * 1) Checks if the @p `event` is legal in the current thread state,
		 * 2) Applies the @p `event` it (bumping this threads `state_change_counter`),
		 * 3) Wakes all waiters,
		 * 4) Calculates the new process state
		 * 5) If the process state changed it calls the `OnStatusChangedCallback`.
		 *
		 * An illegal state change should never happen. If the @p `event` is found illegal in the
		 * current state this function aborts as this means a DVM bug.
		 *
		 * @note: Each VMThread should only apply it's own events, not any other threads events.
		 * Meaning the @p `tid` passed here should be always a TID of the VMThread who actually
		 * wants to perform an event.
		 */
		ApplyResult applyThreadEventOrAbort(api::ThreadID tid, const ThreadEvent& event);

		/**
		 * @brief Prepares a re-run by changing every thread state back to `NotStarted` and clearing
		 * the stop flag.
		 *
		 * @note Only a process that completed successfully may be re-run. If the process was
		 * stopped, killed or panicked in the previous execution the DVM was left in undefined state
		 * and allowing to reset it would be unsafe.
		 *
		 * @return `void` if the reset succeeded, `std::string` with an error if the reset was
		 * refused.
		 */
		std::expected<void, std::string> resetForRun();

		/**
		 * @brief Raises the process-wide stop flag. The aggregate becomes `Stopping` (or `Stopped`
		 * once no thread is active). No-op when the aggregate is already terminal, so a stop can
		 * never overwrite other terminals like `Completed` or `Panicked`.
		 *
		 * @note This only flips the flag - the caller sends out the Stop requests to the threads'
		 * `ThreadSignal`s afterwards.
		 *
		 * @return true if the flag was applied (or already set), false for the terminal no-op.
		 */
		bool requestStop();

		[[nodiscard]] ThreadState threadState(api::ThreadID tid) const;

		[[nodiscard]] ProcessState aggregate() const;

		/**
		 * @brief Per-thread state change counter, bumped on every state change of that thread.
		 * Use with `waitForFreshThreadState` to wait for a transition of a given thread.
		 */
		[[nodiscard]] u64 threadStateChangeCounter(api::ThreadID tid) const;

		/**
		 * @brief Blocks until `pred(snapshot)` holds. Returns the observed process state.
		 */
		template<class Pred>
		requires std::is_invocable_r_v<bool, Pred, const Snapshot&>
		ProcessState waitForProcessState(Pred pred) const {
			std::unique_lock lock(table_mutex);
			const Snapshot   snapshot(*this);
			state_changed.wait(lock, [&] { return pred(snapshot); });
			return agg_state.aggregateState();
		}

		/**
		 * @brief Per-thread `wait`. Blocks until `pred(state(tid))` holds and returns
		 * that state.
		 */
		template<class Pred>
		requires std::is_invocable_r_v<bool, Pred, const ThreadState&>
		ThreadState waitForThreadState(api::ThreadID tid, Pred pred) const {
			std::unique_lock lock(table_mutex);
			const Snapshot   snapshot(*this);
			state_changed.wait(lock, [&] { return pred(snapshot.threadState(tid)); });
			return snapshot.threadState(tid);
		}

		/**
		 * @brief Like `waitForThreadState`, but additionally requires the thread's version to
		 * have advanced past `since` - i.e. waits for a new state of this specific thread.
		 */
		template<class Pred>
		requires std::is_invocable_r_v<bool, Pred, const ThreadState&>
		ThreadState waitForFreshThreadState(api::ThreadID tid, u64 since, Pred pred) const {
			std::unique_lock lock(table_mutex);
			const Snapshot   snapshot(*this);
			state_changed.wait(lock, [&] {
				return snapshot.threadStateChangeCounter(tid) > since
				    && pred(snapshot.threadState(tid));
			});
			return snapshot.threadState(tid);
		}

	private:
		/**
		 * @brief Applies an event, reporting an illegal transition instead of aborting.
		 * @see `applyThreadEventOrAbort` for more info.
		 */
		std::expected<ApplyResult, std::string> applyThreadEvent(
			api::ThreadID tid, const ThreadEvent& event
		);

		/**
		 * @brief The single thread-state write path. Sets the specified thread state and bumps the
		 * thread's state_change_counter.
		 * @note Call under `table_mutex`.
		 */
		void setThreadStateLocked(api::ThreadID tid, ThreadState new_state);

		/**
		 * @brief Shared tail of every function changing the state.
		 * Wakes the waiters and emits the process state change if it changed.
		 *
		 * @note `table_lock` must be the locked `table_mutex` and is released here.
		 */
		void finalizeStateChangeLocked(
			std::unique_lock<std::mutex>& table_lock,
			const ProcessState&           prev,
			const ProcessState&           next
		);

		mutable std::mutex emit_mutex;   ///< Serializes mutations + their emissions. Outer.
		mutable std::mutex table_mutex;  ///< Guards all fields below. Inner.
		mutable std::condition_variable state_changed;

		ProcessStateAggregation agg_state;

		OnStatusChangedCallback on_status_changed;  ///< Accessed under `emit_mutex` only.
	};
}
