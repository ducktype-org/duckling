#pragma once

#include <base/collections/optional.hpp>
#include <base/extend_cpp/scoped_unlock.hpp>
#include <base/types/ints.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/request.hpp>
#include <vm/api/data/response.hpp>
#include <vm/api/data/status.hpp>
#include <vm/core/process/interface_types.hpp>
#include <vm/core/thread/thread_state.hpp>

#include <expected>
#include <string>
#include <thread>

namespace vm {
	class IVMProcess;

	/**
	 * @brief An execution request posted by the VMProcess.
	 * This is serves as a channel to communicate VMProcess with VMThread.
	 */
	enum class ExecutionRequest : std::uint8_t { Resume, Pause, ExecuteOneStep, Stop, NoRequest };

	/**
	 * @brief Common interface for VM thread implementations.
	 * VMThread is responsible for executing the code, while VMProcess is responsible for managing
	 * the process and its resources (like. memory, IO, other threads) while also providing an
	 * interface for the external API.
	 */
	class IVMThread {
	public:
		using ThreadState   = thread_sm::thread_state::ThreadState;
		using ThreadEvent   = thread_sm::thread_event::ThreadEvent;
		using ThreadMachine = thread_sm::ThreadStateMachine;

		explicit IVMThread(api::ThreadID thread_id, IVMProcess& my_process):
			  my_process(my_process),
			  thread_id(thread_id),
			  state_machine(
				  thread_sm::thread_state::NotStarted{}, thread_sm::getThreadStateMachineDefinition()
			  ) {}

		virtual ~IVMThread() = default;

		/**
		 * @brief Returns a copy of the VMThread's current state.
		 */
		[[nodiscard]] thread_sm::thread_state::ThreadState getThreadState() const {
			return state_machine.getStateCopy();
		}

		[[nodiscard]] api::ThreadID getThreadID() const { return thread_id; }

		/**
		 * @brief Direct access to this thread's state machine. Used by `IVMProcess::adoptThread` to
		 * subscribe the thread->process listener.
		 */
		ThreadMachine& getStateMachine() { return state_machine; }

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
		 * @brief Pauses the execution of a program.
		 * Sets the status to paused and waits for the execution thread to respond.
		 * "Assumes execution status is `running`"
		 * @return `void` if and only if program was in the running state and was successfully
		 * paused. Otherwise an error.
		 */
		std::expected<void, std::string> pause();

		/**
		 * @brief Resumes the execution of a program.
		 * Sets the status to running and waits for the execution thread to respond.
		 * "Assumes execution status is `paused`"
		 * @return `void` if and only if program was in the paused state and was successfully
		 * resumed. Otherwise an error.
		 */
		std::expected<void, std::string> resume();

		/**
		 * @brief Execute one step of the program.
		 * Valid only when the VM is paused.
		 * Waits for the program to perform one step and pause.
		 * @return `void` if the program successfully performed one step and paused. Otherwise an
		 * error.
		 */
		std::expected<void, std::string> step();

		/**
		 * @brief End the execution of a program.
		 * Waits for the execution thread to respond.
		 * @return true if the program is in the end stopped.
		 */
		bool stop();

		/**
		 * @brief Non-blocking way of telling the thread to stop.
		 * Used when performing cascade stops on panics etc.
		 */
		void requestStop() noexcept;

		/**
		 * @brief Waits for the execution thread to finish and returns final response.
		 */
		std::expected<api::Response, api::ApiError> join();

		/**
		 * @brief Check if thread has an active execution thread handle.
		 * @return true if exec_thread is active and joinable.
		 */
		[[nodiscard]] bool hasActiveThread() const;


		/**
		 * @brief Returns true if a Pause control request is pending.
		 */
		[[nodiscard]] bool isPauseRequested();

		/**
		 * @brief Returns true if a Stop control request is pending.
		 */
		[[nodiscard]] bool isTerminateRequested();

		/**
		 * @brief Blocks until pred() returns true.
		 *
		 * @warning: Given lock cannot be a lock on external_api_mutex
		 * If you have access to external_api_mutex, implement this yourself.
		 */
		template<class Condition>
		void waitUntilNotPausedAndCondition(std::unique_lock<std::mutex>& lock, Condition condition) {
			pause_cv.wait(lock, [this, &condition] { return !isPauseRequested() && condition(); });
		}

	protected:
		/**
		 * @brief Reports that the thread enters a sleeping state. Sends the `EnterSleep` event and
		 * causes a state change.
		 */
		void reportAsSleeping();

		/**
		 * @brief Reports that the thread left the sleeping state. Sends the `WakeUp{}` event and
		 * causes a state change.
		 */
		void reportAsRunning();

		/**
		 * @brief Handles execution request when `execution_request_break` bool is set.
		 * Used from the thread loop when e.g. the VMProcess requests to pause or stop the execution
		 * while the VMThread is running.
		 */
		void breakActiveExecution();

		/**
		 * @brief Wakes a thread waiting in paused state.
		 */
		void notifyPaused();


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
		 * destructor functions.
		 */
		virtual void execGlobalDestructors() = 0;

		/**
		 * @brief Main debug function that executes one step of the program.
		 */
		virtual void executeOneStep() = 0;

		/**
		 * @brief Calls `run` within safe try-catch block, to catch any exceptions thrown by the
		 * running code and respond to the process with the panicked status.
		 */
		void safeRun(const std::string& func_name, const RunArguments& run_arguments);

		/**
		 * @brief Fires an event from the `exec_thread` context, causes a VMThread state change.
		 */
		void fireEvent(const thread_sm::thread_event::ThreadEvent& event);

		/**
		 * @brief Posts a control request to the VMThread. Called from the `VMProcess` context,
		 * i.e. in `stop`, `pause`, `join`, etc.
		 */
		void postRequest(ExecutionRequest req, bool raise_pending_flag);


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
		 * @brief Joins the OS execution thread if there is one, then clears the handle.
		 */
		void joinExecutionThread();


	private:
		api::ThreadID thread_id;

		/**
		 * @brief State machine handling the inner state of this thread. The only source of truth
		 * about the process state.
		 */
		ThreadMachine state_machine;


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
		std::recursive_mutex        execution_request_mutex;
		ExecutionRequest            execution_request              = ExecutionRequest::NoRequest;
		std::atomic<bool>           execution_request_pending_flag = false;
		std::condition_variable_any pause_cv;
	};
}
