#pragma once

#include <base/collections/optional.hpp>
#include <base/types/ints.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/request.hpp>
#include <vm/api/data/response.hpp>
#include <vm/api/data/status.hpp>
#include <vm/core/process/interface_types.hpp>
#include <vm/core/process/lifecycle.hpp>

#include <expected>
#include <string>
#include <thread>

namespace vm {
	class IVMProcess;

	enum class ExecutionRequest : std::uint8_t { Resume, Pause, ExecuteOneStep, Stop, NoRequest };

	/**
	 * @brief Common interface for VM thread implementations.
	 * VMThread is responsible for executing the code, while VMProcess is responsible for managing
	 * the process and its resources (like. memory, IO, other threads) while also providing an
	 * interface for the external API.
	 */
	class IVMThread {
	public:
		explicit IVMThread(api::ThreadID thread_id, IVMProcess& my_process):
			  my_process(my_process),
			  thread_id(thread_id) {}

		virtual ~IVMThread() = default;

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
		virtual bool spawnThreadAndRun(
			const std::string& func_name, const RunArguments& run_arguments
		);

		/**
		 * @brief Runs a program and waits for it to finish.
		 * Does not create a new thread, runs the program in the current execution thread.
		 */
		virtual void runNoSpawn(const std::string& func_name, const RunArguments& run_arguments);

		/**
		 * @brief Pauses the execution of a program.
		 * Sets the status to paused and waits for the execution thread to respond.
		 * "Assumes execution status is `running`"
		 * @return true if and only if program was in the running state and was successfully paused
		 */
		virtual bool pause();

		/**
		 * @brief Resumes the execution of a program.
		 * Sets the status to running and waits for the execution thread to respond.
		 * "Assumes execution status is `paused`"
		 * @return true if and only if program was in the paused state and was successfully resumed
		 */
		virtual bool resume();

		/**
		 * @brief Execute one step of the program.
		 * Valid only when the VM is paused.
		 * Waits for the program to perform one step and pause.
		 * @return true if the program successfully performed one step and paused
		 */
		virtual bool step();

		/**
		 * @brief End the execution of a program.
		 * Waits for the execution thread to respond.
		 * @return true if the program is in the end stopped.
		 */
		virtual bool stop();
		/**
		 * @brief Check if thread has an active execution thread handle.
		 * @return true if exec_thread is active and joinable.
		 */
		[[nodiscard]] virtual bool hasActiveThread() const;
		/**
		 * @brief Waits for the execution thread to finish and returns final response.
		 */
		virtual std::expected<api::Response, api::ApiError> join();

		virtual bool isPauseRequested();

		virtual bool isTerminateRequested();

		// Given lock cannot be a lock on external_api_mutex
		// If you have access to external_api_mutex, implement this yourself.
		template<class Condition>
		void waitUntilNotPausedAndCondition(std::unique_lock<std::mutex>& lock, Condition condition) {
			pause_cv.wait(lock, [this, &condition] { return !isPauseRequested() && condition(); });
		}

		/**
		 * @brief Handles execution request when `execution_request_break` bool is set.
		 * Used from the thread loop when e.g. the VMProcess requests to pause or stop the execution
		 * while the VMThread is running.
		 */
		virtual void breakActiveExecution();

		/**
		 * @brief Wakes a thread waiting in paused state.
		 */
		virtual void notifyPaused();

		[[nodiscard]] api::ProcStatus getStatus() const { return status_machine.getStateCopy(); }

		/**
		 * @brief Blocks until this thread is paused or its execution reached a
		 * terminal status (completed, stopped, panicked), and the additional
		 * condition holds.
		 *
		 * The extra condition lets callers require consistency with state that the
		 * executing thread publishes around its own transition (e.g. the process
		 * status, which is updated before this machine's waiters are notified):
		 * the predicate is re-evaluated on the post-dispatch notification, so a
		 * caller that races into the wait mid-dispatch does not return early.
		 *
		 * @return The status that satisfied the wait.
		 */
		template<class Condition>
		api::ProcStatus waitUntilPausedOrTerminated(const Condition& condition) const {
			return status_machine
			    .waitUntil([&condition](const api::ProcStatus& status, u64) {
					return (std::holds_alternative<api::Paused>(status)
				            || api::isStatusTerminal(status))
				        && condition();
				})
			    .state;
		}

		api::ProcStatus waitUntilPausedOrTerminated() const {
			return waitUntilPausedOrTerminated([] { return true; });
		}

		[[nodiscard]] api::ThreadID getThreadID() const { return thread_id; }

		[[nodiscard]] virtual u64 getNumberOfCurrentStackFrames() const = 0;

		IVMProcess& getMyProcess() { return my_process; }

		[[nodiscard]] const IVMProcess& getMyProcess() const { return my_process; }

		/**
		 * @brief Returns the value of the execution_request_pending_flag atomic boolean.
		 * @note This is meant to be periodically check e.g. in the execution loop.
		 */
		[[nodiscard]] const std::atomic<bool>& getExecutionRequestPendingFlag() const {
			return execution_request_pending_flag;
		}

	protected:
		/**
		 * @brief Execution thread handle shared by IVMThread implementations.
		 */
		base::Optional<std::thread> exec_thread;

		IVMProcess& my_process;

		/**
		 * @brief Run a single function with given parameters.
		 */
		virtual void run(const std::string& func_name, const RunArguments& run_arguments) = 0;

		/**
		 * @brief Calls `run` within safe try-catch block, to catch any exceptions thrown by the
		 * running code and respond to the process with the panicked status.
		 */
		virtual void safeRun(const std::string& func_name, const RunArguments& run_arguments);

		virtual void executeOneStep() = 0;

		/**
		 * @brief Applies a lifecycle event to this thread's status machine and announces
		 * it to the owning VMProcess.
		 *
		 * This is how the executing thread publishes its state: the thread machine is
		 * the single source of truth for this thread's status, and every transition
		 * wakes API callers blocked in `pause`/`resume`/`step`/`stop`/`spawnThreadAndRun`.
		 * The process applies its own main/child-thread policy to the announced event
		 * (see `IVMProcess::applyEvent`).
		 */
		void dispatchEvent(const lifecycle::Event& event);

		/**
		 * @brief Main function of the VMThread "debug" mode, where the step by step execution can
		 * take place. After each step the execution status is set to `paused` and the VMThread
		 * waits for the next command. Mutex "execution_request_mutex" is held when the VM is
		 * executing the code.
		 *
		 * @param lock
		 */
		void runDebuggerLoop(std::unique_lock<std::mutex>& lock);

		/**
		 * @brief Function to be called when the VMThread hits a breakpoint.
		 */
		void handleBreakpoint();

		/**
		 * @brief Joins the execution thread if it is joinable.
		 */
		bool joinExecutionThread();

		/**
		 * @brief Applies a lifecycle event to this thread's status machine only,
		 * without announcing it to the VMProcess. Used for thread-local transitions
		 * like recycling a joined thread (`Reset`), which must not touch the
		 * process-wide status.
		 */
		void applyThreadEvent(const lifecycle::Event& event);

		/**
		 * @brief Panics if `result` indicates that the event was rejected by this
		 * thread's status machine - the executing thread is the only writer of its
		 * own status, so a rejected event is a transition-table bug.
		 */
		void assertLifecycleEventApplied(
			const lifecycle::Machine::ResultT& result, const lifecycle::Event& event
		) const;

		/**
		 * @brief Function to be called when the VMProcess is deinitialized. Calls GlobalData's
		 * destructor functions.
		 */
		virtual void execGlobalDestructors() = 0;

	private:
		/**
		 * @brief Lifecycle state machine holding the status of this thread.
		 * See `lifecycle.hpp` for the allowed transitions. Successful transitions
		 * are versioned, which lets API callers wait for a response to their
		 * execution requests without a separate response channel.
		 */
		lifecycle::Machine status_machine{ api::NotStarted{}, &lifecycle::statusTransitions() };

		api::ThreadID thread_id;

		/**
		 * @brief Mutex responsible for setting the execution_request and execution_request_break
		 * flags.
		 *
		 * This flags are used to signal the requests from the VMProcess to the VMThread to perform
		 * an action like pause, resume, stop.
		 *
		 * As an optimization, when the VMThread is running and VMProcess want to break its
		 * execution (by requesting pause or stop), it sets the execution_request_break flag to
		 * true, so the running VMThread can only check this flag first and not acquire the mutex.
		 */
		std::mutex        execution_request_mutex;
		ExecutionRequest  execution_request              = ExecutionRequest::NoRequest;
		std::atomic<bool> execution_request_pending_flag = false;

		/**
		 * @brief Serializes client-side execution requests (pause/resume/step/stop),
		 * held across the request and the wait for its response.
		 *
		 * Without it, concurrent clients interleave on the single execution_request
		 * slot: e.g. one client's Resume consumes the pause another client's Step is
		 * waiting to observe, leaving the stepper waiting forever. Each request is
		 * individually bounded (it completes or fails fast), so serializing them
		 * keeps every caller bounded as well. The executing thread never takes this
		 * mutex.
		 */
		std::mutex api_request_mutex;

		std::condition_variable pause_cv;
	};
}
