#pragma once

#include "blocking_queue.hpp"
#include "low_program/instruction.hpp"

#include <base/box.hpp>
#include <base/ints.hpp>
#include <base/optional.hpp>

#include <vm/api/data/core_operation_error.hpp>
#include <vm/api/data/request.hpp>
#include <vm/api/data/response.hpp>
#include <vm/api/data/status.hpp>
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
	enum class ExecutionRequest : std::uint8_t { Resume, Pause, ExecuteOneStep, Stop, NoRequest };
	enum class ExecutionResponse : std::uint8_t {
		Running,
		Paused,
		ExecutionStopped,
		ExecutionCompleted,
		ExecutionPanicked
	};

	struct Frame;

	class VMProcess;

	/**
	 * @brief Frames are on stack, this is the maximum number of frame pointers available.
	 */
	constexpr u64 FRAME_COUNT = 16'384;

	/**
	 * @brief Number of fixed and preallocated stack bytes.
	 * 256 - a magic number - it means if all of the frames take on average 256 bytes
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

		/**
		 * This is currently duplicated inside VCPUStatus
		 */
		api::ExecStatus status = api::NotStarted{};

		/**
		 * @brief Process link as well as some of it's resources.
		 */
		VMProcess& process;
		Memory&    process_memory;

		// This might change:
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
		 * true, so the running VMThread can only check this flag first and not aquire the mutex.
		 */
		std::mutex        execution_request_mutex;
		ExecutionRequest  execution_request       = ExecutionRequest::NoRequest;
		std::atomic<bool> execution_request_break = false;

		/**
		 * @brief Message queue to send responses to the VMProcess.
		 * @todo rewrite this to C++ futures
		 */
		BlockingQueue<ExecutionResponse> execution_response_queue;

		bool waitForBrakepointResponse();

		bool waitForStoppedResponse();

		bool waitForRunningResponse();

		void respondExecutionRequest(ExecutionResponse response);

		void executeOneStep();
		
		void initializeMainLocalStack(Frame* frame, const std::vector<std::string>& args);

		/**
		 * @brief @TODO:
		 * get loaded code from VCPU when possible
		 */
		MCRef<low::LowVMProgram> executing_program = nullptr;

		/**
		 * @TODO:
		 * following modifications should be made in the future:
		 * - Error handling done by throwing ()
		 * - Setup for execution recovery
		 * - This functions currently can deref only simple pointers, and always return
		 * view to data pointed by pointer. This does not take into consideration possibility
		 * of derefing only part of a block with given type from given offset.
		 */
		base::ModRawView internalDerefPointer(Pointer);

		/**
		 * @brief This is the main function to call when starting the execution of a program.
		 *
		 * @return value returned by the program
		 */
		u64 internalCallFunction(CRef<low::FuncData> program, const std::vector<std::string>& args);

		void setProcessStatus(const vm::api::ExecStatus& status);

	public:
		VMThread(VMProcess& process);

		void breakActiveExecution();

		void handlePausedExecution(std::unique_lock<std::mutex>&);

		void handleBreakpoint();

		/**
		 * @brief Creates new thread that runs the code in the Executor service.
		 * Blocks until the thread is running.
		 * @param code
		 * @return true if the thread was successfully created and the program is running
		 */
		bool
			initThreadAndRun(CRef<low::LowVMProgram> program, const std::vector<std::string>& args);

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
		 * @return true if and only if program was in the paused state and was successfully resumed
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
		 * Waits for the execution thread to responde.
		 * @return true if the program is in the end stopped.
		 */
		bool stop();

		/**
		 * @brief Run the program.
		 */
		void run(CRef<low::LowVMProgram> program, const std::vector<std::string>& args);

		std::expected<api::Response, api::CoreOperationError> getCurrentPosition();

		// Given lock cannot be a lock on external_api_mutex
		// If you have access to external_api_mutex, implement this yourself.
		template<class Condition>
		void waitUntilNotPausedAndCondition(
			std::unique_lock<std::mutex>& lock, Condition condition
		) {
			pause_cv.wait(lock, [this, &condition] { return !isPauseRequested() && condition(); });
		}

		void notifyPaused();


		bool isPauseRequested();
		bool isTerminateRequested();

		friend class VMProcess;
		friend class OpFuns;
	};
}
