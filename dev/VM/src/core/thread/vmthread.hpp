#pragma once

#include "base/box.hpp"
#include "base/optional.hpp"
#include <core/process/memory/allocator/allocator.hpp>
#include <core/process/memory/allocator/stack_allocator.hpp>
#include <condition_variable>
#include <core/process/memory/memory.hpp>
#include <core/process/type_metadata/type_metadata.hpp>
#include <code_data/instruction.hpp>
#include <code_data/code.hpp>

#include <api/data/request.hpp>
#include <api/data/status.hpp>

#include <mutex>
#include <atomic>

/**
 * For now only single threaded execution is suported
 */


namespace vm {
	enum class ExecutionStrategy { Normal, StepByStep, Paused, Stoped };
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

	// This structure holds pointers to `frame_stack` and `local_stack_reserved`
	// vectors for fast access during runtime. `frame_stack` is a vector of frames,
	// that we use like a stack. Top of the stack is saved in the `frame` argument
	// passed inside opcode functions, which is also the current frame. `local_stack_reserved` is
	// one continuous block of memory, from which every function gets it's own chunk. It also
	// behaves like a stack, but can be moved forward by many bytes, so `local_stack_top`
	// is kept to remember where the top of the stack currently is.
	struct RuntimeData {
		Frame*     frame_stack_base;  // Pointer to the first frame from `frame_stack` vector.
		Frame*     frame_stack_end;   // Pointer to the first value not allocated.
		std::byte* local_stack_base;  // Pointer to the start of `local_stack_reserved`.
		std::byte* local_stack_top;   // Pointer to the place, where new stack should start.
		std::byte* local_stack_end;   // Pointer to the first value not allocated.

		RuntimeData(std::vector<Frame>& frame_stack, std::vector<std::byte>& local_stack):
			  frame_stack_base(frame_stack.data()),
			  frame_stack_end(frame_stack.data() + FRAME_COUNT),
			  local_stack_base(local_stack.data()),
			  local_stack_top(local_stack.data()),
			  local_stack_end(local_stack.data() + STACK_LENGTH) {}
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
	class VMThread {
	private:
		base::Optional<std::thread> exec_thread;

		std::vector<Frame> frame_stack;

		/**
		 * @brief Continuous block of memory, that is used for the call stack.
		 * Here each stack frame is composed of: "arg_stack", local_stack". The "arg_stack" is used
		 * for arguments passed to the function, and the "local_stack" is used for local variables.
		 * [arg_stack(1) | local_stack(1) | arg_stack(1) | local_stack(2) | ...]
		 * When preparing for a new function call, the arguments are placed
		 * exactly after the local variables of the current function, so in the "arg_stack"
		 * of the new frame.
		 */
		std::vector<std::byte> local_stack_reserved;

		RuntimeData runtime_data;

		/**
		 * This is currently duplicated inside VCPUStatus
		 */
		api::ExecStatus status;

		/**
		 * @brief Process link as well as some of it's resources.
		 */
		VMProcess&      process;
		Memory&         process_memory;
		TypeMetadata&   process_types;
		StackAllocator& process_stack_allocator;
		Allocator&      process_dynamic_allocator;


		// This might change:
		std::condition_variable pause_cv;
		/**
		 * @brief Mutex that controls access to `is_running` and `execution_strategy`.
		 *
		 * When the supervisor thread (external api) wants to change the execution strategy, it has
		 * to lock this mutex. The Execution Thread running in a loop will first check the
		 * `is_running` flag every instruction, and if it is false, it will wait on `pause_cv` until
		 * it is notified by the supervisor thread.
		 */
		std::mutex        external_api_mutex;
		ExecutionStrategy execution_strategy = ExecutionStrategy::Stoped;
		// @todo change to atomic_flag
		std::atomic<bool> is_running = false;

		// This function is marked as cold, because, well, it is cold, but
		// the compiler did not figure this out on its own, hence the
		// attribute. In short, this makes the compiler emit assembly with
		// the assumption this method is rarely called. Testing has shown this
		// speeds things up significantly.
		[[gnu::cold]]
		void handleExecutionStrategy();
		/**
		 * @brief This function is called to check the `is_running` atomic bool.
		 * If it is false, it will call `handleExecutionStrategy`.
		 */
		void handleExecutionStrategyIfNeeded();

		/**
		 * @brief @TODO:
		 * get loaded code from VCPU when possible
		 */
		MRef<const Code> executing_code = nullptr;

		/**
		 * @TODO:
		 * following modifications should be made in the future:
		 * - Error handling done by throwing (for efficiency)
		 * - Setup for execution recovery
		 * - This functions currently can deref only simple pointers, and always return
		 * view to data pointed by pointer. This does not take into consideration possibility
		 * of derefing only part of a block with given type from given offset.
		 */
		base::ModRawView internalDerefPointer(Pointer);

		/** Using raw Frame pointers seem to boost performance in function calls */
		Frame internalInitFrame();

		/**
		 * @brief This is the main function to call when starting the execution of a program.
		 *
		 * @return value returned by the program
		 */
		u64 internalCallMain(const FuncData&);

		// @TODO add some thread data in the future

		void setStatus(vm::api::ExecStatus status);

	public:
		VMThread(VMProcess& process);

		/**
		 * @brief Pause the execution of a program (by Supervisor)
		 * Set execution status to paused.
		 * "Assumes execution status is `running`"
		 *
		 * This function should return only when the execution is paused
		 * This function may be called at any state of the execution
		 * @return true if and only if program was in the running state and was successfully paused
		 */
		bool pause();

		/**
		 * @brief Resume the execution of a program (by Supervisor)
		 * Set execution status to `running`.
		 * "Assumes execution status is `paused`"
		 *
		 * Function should return only when the execution is resumed
		 * This function may be called at any state of the execution
		 * @return true if and only if program was in the paused state and was successfully resumed
		 */
		bool resume();

		bool step();

		/**
		 * @brief Force kill the execution of a thread.
		 * Called from the process.
		 */
		void stop();

		void prestart();

		void initThread(Ref<const vm::Code> code);

		/**
		 * @brief Called on coreThread
		 * coreThread is `main` exec thread
		 */
		void run(Ref<const Code>);

		// Given lock cannot be a lock on external_api_mutex
		// If you have access to external_api_mutex, implement this yourself.
		template<class Condition>
		void waitUntilNotPausedAndCondition(
			std::unique_lock<std::mutex>& lock, Condition condition
		) {
			pause_cv.wait(lock, [this, &condition] { return !isPaused() && condition(); });
		}

		void notifyPaused();

		bool isPaused();
		bool isAlive();

		friend class VMProcess;
		friend class OpFuns;
	};
}
