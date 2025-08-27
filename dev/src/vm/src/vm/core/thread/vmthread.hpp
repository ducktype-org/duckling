#pragma once

#include "blocking_queue.hpp"
#include "low_program/instruction.hpp"
#include "vmvalue.hpp"

#include <base/box.hpp>
#include <base/ints.hpp>
#include <base/optional.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/request.hpp>
#include <vm/api/data/response.hpp>
#include <vm/api/data/status.hpp>
#include <vm/core/process/interface_types.hpp>
#include <vm/core/process/memory/memory.hpp>
#include <vm/core/process/memory/thread_stack.hpp>
#include <vm/core/process/type_metadata/type_metadata.hpp>
#include <vm/core/thread/low_program/low_program.hpp>

#include <atomic>
#include <condition_variable>
#include <mutex>

/**
 * For now only single threaded execution is suported
 */

namespace vm {
	// Forward declarations
	namespace builtins {
		class FunctionHandlers;
	}

	struct Frame;

	class VMProcess;

	enum class ExecutionRequest : std::uint8_t { Resume, Pause, ExecuteOneStep, Stop, NoRequest };


	/**
	 * @brief Frames are on stack, this is the maximum number of frame pointers available.
	 */
	constexpr u64 FRAME_COUNT = 16'384;

	/**
	 * @brief Number of fixed and preallocated stack bytes.
	 * 256 - a magic number - it means if frames take on average 256 bytes
	 * of stack space, then there can be at most FRAME_COUNT frames
	 * on the stack, but if functions on average take more than 256 bytes of space
	 * then fewer frames will be able to fit.
	 */
	constexpr u64 STACK_LENGTH = FRAME_COUNT * 256;

	/**
	 * @brief This structure holds pointers to `frame_stack` and `local_stack_reserved`
	 * vectors for fast access during runtime. `frame_stack` is a vector of frames,
	 * that we use like a stack. Top of the stack is saved in the `frame` argument
	 * passed inside opcode functions, which is also the current frame. `local_stack_reserved` is
	 * one continuous block of memory, from which every function gets it's own chunk. It also
	 * behaves like a stack, but can be moved forward by many bytes, so `local_stack_top`
	 * is kept to remember where the top of the stack currently is.
	 */
	struct RuntimeData {
		Frame* frame_stack_base;      /// Pointer to the first frame from `frame_stack` vector.
		Frame* frame_stack_end;       /// Pointer to the first value not allocated.
		Frame* frame_stack_current;   /// Pointer to the current frame - used only when debugging.
		std::byte* local_stack_base;  /// Pointer to the start of `local_stack_reserved`.
		std::byte* local_stack_end;   /// Pointer to the first value not allocated.

		RuntimeData(Ref<ThreadStack> stack):
			  frame_stack_base(stack->getFrameStack()->data()),
			  frame_stack_end(stack->getFrameStack()->data() + stack->getFrameStack()->size()),
			  frame_stack_current(stack->getFrameStack()->data()),
			  local_stack_base(stack->getLocalStack()->data()),
			  local_stack_end(stack->getLocalStack()->data() + stack->getLocalStack()->size()) {}
	};

	/**
	 * @brief Service that executes the code.
	 *
	 * This service is responsible for executing the code.
	 * Most of the code in this class is executed
	 * in the Execution Thread, but some methods can be called from the supervisor thread.
	 * It also provides endpoints for the VCPU to control the execution of the code
	 * in a memory-safe way (see `external_api_mutex`).
	 */
	class VMThread final {
	private:
		base::Optional<std::thread> exec_thread;

		RuntimeData runtime_data;

		api::ProcStatus status = api::NotStarted{};

		/**
		 * @brief Link to parent process.
		 */
		VMProcess& process;

		/**
		 * @brief Parent process'es memory.
		 */
		Memory& process_memory;

		std::condition_variable pause_cv;

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
		ExecutionRequest  execution_request       = ExecutionRequest::NoRequest;
		std::atomic<bool> execution_request_break = false;

		/**
		 * @brief Stores exit value of the last ran function. ExecutionCompleted exec status can
		 * store a reference to this object.
		 */
		base::Optional<Ref<VmValue>> exit_value_storage{};

		/**
		 * @brief Message queue to send responses to the VMProcess.
		 * @todo rewrite this to C++ futures
		 */
		BlockingQueue<api::ProcStatus> execution_response_queue;

		bool waitForBreakpointResponse();

		bool waitForStoppedResponse();

		bool waitForRunningResponse();

		void respondExecutionRequest(const api::ProcStatus& response);

		void executeOneStep();

		/**
		 * @brief Creates a list of instructions, which initialize the argv table and populate it
		 * with given command line `args`, push the argc and *argv blocks onto mains local stack,
		 * perform the call and deinitialize the argv table when main returns.
		 */
		[[nodiscard]] low::FuncData createProgramStartFunction(
			const low::FuncData& func, const ProgramRunArguments& args
		) const;

		/**
		 * @brief Creates a list of instructions, which push the passed `func_args` onto the local
		 * stack and perform a call to `func`.
		 * @note `func_args` should be changed to a vector of arguments of any VM type.
		 * This should be changed after: https://github.com/ducktype-org/duckling/issues/721.
		 */
		[[nodiscard]] low::FuncData createStartFunctionFor(
			const low::FuncData& func, const FunctionRunArguments& func_args
		) const;

		/**
		 * @brief Holds the currently executed program
		 */
		MCRef<low::LowVMProgram> executing_program = nullptr;

		/**
		 * @brief This is the primary function to call to start execution on the VM.
		 * It calls both the main function when running the program and single functions called by
		 * the `runFunction` endpoint. It starts the execution beginning with the first instruction
		 * in the start_function bytecode vector.
		 * @param start_function - the code of the start function.
		 * @param func - the function to execute.
		 * @return Mutable reference to a value returned by the program
		 */
		Ref<VmValue> executeFunction(const low::FuncData& start_function, const low::FuncData& func);

		void setProcessStatus(const vm::api::ProcStatus& status);

		void handleBreakpoint();

		void handlePausedExecution(std::unique_lock<std::mutex>&);


	public:
		VMThread(VMProcess& process);

		void breakActiveExecution();

		/**
		 * @brief Creates a new thread that runs the code in the Executor service.
		 * Blocks until the thread is running.
		 *
		 * @param program - program for the thread to run,
		 * @param func_name - name of the function to run,
		 * @param run_arguments - if running a function (not a whole program), these are the
		 * arguments to pass as parameters to the function,
		 * @param run_arguments - if running a program, these are the command line arguments passed
		 * to the program (argv equivalent).
		 *
		 * @return true if the thread was successfully created and the program is running, false if
		 * there is already a thread running.
		 */
		bool spawnThreadAndRun(
			CRef<low::LowVMProgram> program,
			const std::string&      func_name,
			const RunArguments&     run_arguments
		);

		/**
		 * @brief Pauses the execution of a program.
		 * Sets the status to paused and waits for the execution thread to respond.
		 * "Assumes execution status is `running`"
		 * @return true if and only if program was in the running state and was successfully paused
		 */
		bool pause();

		/**
		 * @brief Resumes the execution of a program.
		 * Sets the status to running and waits for the execution thread to respond.
		 * "Assumes execution status is `paused`"
		 * @return true if and only if program was in the paused state and was successfully
		 * resumed
		 */
		bool resume();

		/**
		 * @brief Execute one step of the program.
		 * Valid only when the VM is paused.
		 * Waits for the program to perform one step and pause.
		 * @return true if the program successfully performed one step and paused
		 */
		bool step();

		/**
		 * @brief End the execution of a program.
		 * Waits for the execution thread to respond.
		 * @return true if the program is in the end stopped.
		 */
		bool stop();

		/**
		 * @brief Run a single function with given parameters.
		 */
		void run(
			CRef<low::LowVMProgram> program,
			const std::string&      func_name,
			const RunArguments&     run_arguments
		);

		/**
		 * @brief Function to be called when the VMProcess is deinitialized. Calls GlobalData's
		 * destructor functions.
		 */
		void execGlobalDestructors();

		std::expected<api::Response, api::ApiError> getCurrentPosition();

		// Given lock cannot be a lock on external_api_mutex
		// If you have access to external_api_mutex, implement this yourself.
		template<class Condition>
		void waitUntilNotPausedAndCondition(std::unique_lock<std::mutex>& lock, Condition condition) {
			pause_cv.wait(lock, [this, &condition] { return !isPauseRequested() && condition(); });
		}

		void notifyPaused();

		bool isPauseRequested();

		bool isTerminateRequested();

		friend class VMProcess;
		friend class OpFuns;
		friend class builtins::FunctionHandlers;
	};
}
