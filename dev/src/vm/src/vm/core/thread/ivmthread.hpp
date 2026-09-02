#pragma once

#include <base/collections/optional.hpp>
#include <base/types/ints.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/request.hpp>
#include <vm/api/data/response.hpp>
#include <vm/api/data/status.hpp>
#include <vm/core/process/interface_types.hpp>
#include <vm/core/thread/thread_signal.hpp>
#include <vm/core/thread/thread_state.hpp>

#include <expected>
#include <mutex>
#include <string>
#include <thread>

namespace vm {
	class IVMProcess;
	class ProcessStateManager;

	/**
	 * @brief Common interface for VM thread implementations.
	 * VMThread is responsible for executing the code, while VMProcess is responsible for managing
	 * the process and its resources (like memory, IO, other threads) while also providing an
	 * interface for the external API.
	 */
	class IVMThread {
	public:
		using ThreadState = thread_state::ThreadState;
		using ThreadEvent = thread_event::ThreadEvent;

		IVMThread(api::ThreadID thread_id, IVMProcess& my_process);

		virtual ~IVMThread() = default;

		/**
		 * @brief Returns a copy of the VMThread's current state, read from the process's state
		 * table.
		 */
		[[nodiscard]] ThreadState getThreadState() const;

		[[nodiscard]] api::ThreadID getThreadID() const { return thread_id; }

		/**
		 * @brief Creates a new thread that runs the code.
		 * Blocks until the thread is not running.
		 *
		 * @param func_name - name of the function to run,
		 * @param run_arguments - if running a function (not a whole program), these are the
		 * arguments to pass as parameters to the function,
		 * @param run_arguments - if running a program, these are the command line arguments passed
		 * to the program (argv equivalent).
		 *
		 * @return true if the thread was successfully created and the program is running, false if
		 * there is already a thread running.
		 */
		bool spawnThreadAndRun(const std::string& func_name, const RunArguments& run_arguments);

		/**
		 * @brief Runs a program and waits for it to finish.
		 * Does not create a new thread, runs the program in the current execution thread.
		 *
		 * @return true if the program was run, false if this thread is already executing.
		 */
		[[nodiscard]] bool runNoSpawn(
			const std::string& func_name, const RunArguments& run_arguments
		);

		/**
		 * @brief Posts a Pause request and returns without waiting for the thread to pause.
		 *
		 * @return `void` iff the request was accepted or the reason why it was refused otherwise.
		 */
		[[nodiscard]] std::expected<void, std::string> requestPause();

		/**
		 * @brief Blocks until this thread is `Paused` (or terminal).
		 *
		 * @note A thread sleeping on IO/Mutex cannot pause while asleep, so this blocks for as long
		 * as the IO does. The caller must hold no lock that `api::input` or `api::stop` needs.
		 *
		 * @return `void` iff the thread ended up `Paused` or an error otherwise.
		 */
		[[nodiscard]] std::expected<void, std::string> awaitPause();

		/**
		 * @brief Resumes the execution of this (paused) thread.
		 * Posts a Resume request and blocks until the thread has committed a fresh state (the
		 * resume, or its termination).
		 * @return `void` iff the request was accepted, an error otherwise.
		 */
		std::expected<void, std::string> resume();

		/**
		 * @brief Executes one step of this (paused) thread and blocks until it is `Paused`
		 * again (or terminal).
		 * @return `void` iff the request was accepted; an error otherwise.
		 */
		std::expected<void, std::string> step();

		/**
		 * @brief Non-blocking way of telling the thread to stop.
		 * Used when performing cascade stops on panics etc.
		 */
		void requestStop() noexcept;

		/**
		 * @brief Waits for the exec thread to finish and returns its exit value.
		 * @return The exit value of the thread if it completed normally, an API error otherwise
		 * (the thread was never started, it panicked or it was stopped before completing).
		 */
		std::expected<api::Response, api::ApiError> join();

		/**
		 * @brief Check if thread has an active execution thread handle.
		 * @return true if exec_thread is active and joinable.
		 */
		[[nodiscard]] bool hasActiveThread() const;

		/**
		 * @brief Decides whether an API control request can be performed in current thread state.
		 *
		 * @return Nothing if the request may be performed or the reason why it can't.
		 */
		[[nodiscard]] std::expected<void, std::string> validateThreadRequest(
			ThreadSignal::Request request
		) const;

		/**
		 * @brief Returns true if a Stop request is pending.
		 */
		[[nodiscard]] bool isTerminateRequested() const { return signal.stopRequested(); }

		/**
		 * @brief Returns true if a control request is pending and the interpreter loop should
		 * break execution.
		 */
		[[nodiscard]] bool isBreakRequested() const { return signal.breakRequested(); }

		/**
		 * @brief Interruptable wait over a given lock (e.g. the process IO lock).
		 * Returns when `condition()` holds or a Stop was posted for this thread.
		 * Check `isTerminateRequested()` afterwards to distinguish the two.
		 */
		template<class Lock, class Condition>
		void waitInterruptible(Lock& lock, Condition condition) {
			signal.waitInterruptible(lock, std::move(condition));
		}

	protected:
		/**
		 * @brief Reports that the thread enters a sleeping state (i.e. blocking on IO).
		 */
		void reportAsSleeping();

		/**
		 * @brief Reports that the thread left the sleeping state.
		 */
		void reportAsRunning();

		/**
		 * @brief Handles the pending control request when `isBreakRequested()` is observed by
		 * the interpreter loop. Stop unwinds via `KillProcessException`, Pause enters the
		 * paused loop, a stale Resume/Step is dropped.
		 */
		void breakActiveExecution();

		/**
		 * @brief Releases the GIL if it's taken.
		 *
		 * @note Used to release the GIL when this thread pauses, so others may go.
		 */
		virtual void releaseGilIfHeld() {}

		/**
		 * @brief Reacquires the GIL if it's not taken.
		 *
		 * @note Used to reacquire the GIL when this thread performs a `step`, `stop`, `resume` in
		 * the debugger loop.
		 */
		virtual void acquireGilIfNotHeld() {}

		/**
		 * @brief Wakes this thread if it is blocked in `waitInterruptible` - i.e. re-evaluates
		 * the condition it is waiting for without posting any control request.
		 *
		 * Used when the process changes state the thread may be waiting on.
		 */
		void notifyWaiters();

		[[nodiscard]] virtual u64 getNumberOfCurrentStackFrames() const = 0;

		IVMProcess& getMyProcess() { return my_process; }

		[[nodiscard]] const IVMProcess& getMyProcess() const { return my_process; }

	protected:
		/**
		 * @brief Execution thread handle shared by IVMThread implementations.
		 */
		mutable std::mutex          exec_thread_mutex;
		base::Optional<std::thread> exec_thread;

		/**
		 * @brief The process this VMThread belongs to.
		 */
		IVMProcess& my_process;

		/**
		 * @brief Run a single function with given parameters.
		 */
		virtual void run(const std::string& func_name, const RunArguments& run_arguments) = 0;

		/**
		 * @brief Function to be called when the VMProcess is deinitialized. Calls GlobalData's
		 * destructor functions. Can throw anything the normal interpreter does.
		 */
		virtual void execGlobalDestructors() = 0;

		/**
		 * @brief Main debug function that executes one step of the program.
		 */
		virtual void executeOneStep() = 0;

		/**
		 * @brief Returns true if the instruction the thread is paused on ends the execution (is an
		 * MicroOpcode::Exit).
		 */
		[[nodiscard]] virtual bool isAtExecutionEnd() const = 0;

		/**
		 * @brief Calls `run` within a safe try-catch block, to catch any exceptions thrown by
		 * the running code and commit the corresponding terminal transition (Kill/Panic).
		 * @note `Spawn` is committed by the spawner before this runs (see `prepareSpawnLocked`).
		 */
		void safeRun(const std::string& func_name, const RunArguments& run_arguments);

		/**
		 * @brief Applies a state transition of this thread in the process's `ProcessStateManager`.
		 * Fatal on an illegal transition, as this means a fatal DVM internal error.
		 */
		void applyEvent(const ThreadEvent& event);

		/**
		 * @brief Function to be called when the VMThread hits a breakpoint.
		 */
		void handleBreakpoint();

		/**
		 * @brief Joins the OS execution thread if there is one, then clears the handle.
		 */
		void joinExecutionThread();

	private:
		/**
		 * @brief The process's state manager (this thread's state lives there).
		 */
		[[nodiscard]] ProcessStateManager&       getProcessStateManager();
		[[nodiscard]] const ProcessStateManager& getProcessStateManager() const;

		/**
		 * @brief Spawn preparation, called under `exec_thread_mutex` while the thread is
		 * non-active. Rejects a busy thread, clears the signal slot and commits `Spawn` (the one
		 * process-side write of the thread state, legal because no exec thread exists yet).
		 * @return true if the spawn was claimed.
		 */
		bool prepareSpawnLocked();

		/**
		 * @brief The paused state of the exec thread. Commits `Paused`, then consumes control
		 * requests until one of Resume (returns), Stop (throws `KillProcessException`) or Step
		 * (executes one step - with no lock held - and re-pauses).
		 */
		void pausedLoop();

		api::ThreadID thread_id;

		/**
		 * @brief The process/API -> exec-thread control request channel.
		 */
		ThreadSignal signal;
	};
}
