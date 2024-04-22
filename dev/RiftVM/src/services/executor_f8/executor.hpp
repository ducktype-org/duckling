#pragma once

#include "../services.hpp"
#include "code_data/instruction.hpp"
#include "code_data/code.hpp"
#include "kill_core_exception.hpp"
#include "services/executor_f8/op_case.hpp"

#include <code_data/code.hpp>
#include <api/data/request.hpp>
#include <api/data/status.hpp>

#include <iostream>
#include <shared_mutex>
#include <mutex>
#include <condition_variable>
#include <atomic>

/**
 * For now only single threaded execution is suported
 */


namespace vm {
	enum class ExecutionStrategy { Normal, StepByStep, Paused, Stoped };
	struct Frame;

	const u64 FRAMES_LENGTH = 16'384;
	const u64 STACK_LENGTH  = FRAMES_LENGTH * 256;

	struct RuntimeData {
		Frame*     frame_stack_base;
		Frame*     frame_stack_end;
		std::byte* local_stack_base;
		std::byte* local_stack_top;
		std::byte* local_stack_end;

		RuntimeData(std::vector<Frame>& frame_stack, std::vector<std::byte>& local_stack):
			  frame_stack_base(frame_stack.data()),
			  frame_stack_end(frame_stack.data() + FRAMES_LENGTH),
			  local_stack_base(local_stack.data()),
			  local_stack_top(local_stack.data()),
			  local_stack_end(local_stack.data() + STACK_LENGTH) {}
	};

	class Executor {
	private:
		Allocator&      dynamic_allocator;
		StackAllocator& stack_allocator;
		Memory&         memory;
		TypeMetadata&   types;

		VCPU& vcpu;

		std::vector<Frame>     frame_stack;
		std::vector<std::byte> local_stack;
		RuntimeData            runtime_data;

		/**
		 * This is currently duplicated inside VCPUStatus
		 */
		api::ExecStatus status;

		template<class... DynamicServices>
		Executor(ServiceManagerDef<DynamicServices...>& serviceManager):
			  dynamic_allocator(serviceManager.template get<Allocator>()),
			  stack_allocator(serviceManager.template get<StackAllocator>()),
			  memory(serviceManager.getVCPU().getData().template get<Memory>()),
			  types(serviceManager.getVCPU().getData().template get<TypeMetadata>()),
			  vcpu(serviceManager.getVCPU()),
			  frame_stack(FRAMES_LENGTH, internalInitFrame()),
			  local_stack(STACK_LENGTH),
			  runtime_data(frame_stack, local_stack) {
			// @TODO: not loaded status
			setStatus(api::NotStarted{});
		}

		// This might change:
		std::condition_variable pause_cv;
		std::mutex              external_api_mutex;
		ExecutionStrategy       execution_strategy = ExecutionStrategy::Stoped;
		std::atomic<bool>       is_running         = false;

		// This function is marked as cold, because, well, it is cold, but
		// the compiler did not figure this out on its own, hence the
		// attribute. In short, this makes the compiler emit assembly with
		// the assumption this method is rarely called. Testing has shown this
		// speeds things up significantly.
		[[gnu::cold]]
		void handleExecutionStrategy();
		void handleExecutionStrategyIfNeeded();

		/**
		 * @brief @TODO:
		 * get loaded code from VCPU when possible
		 */
		const Code* executing_code = nullptr;

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

		inline void initNextFrame(Frame*, StandardFunctionArgs& args);
		/** Using raw Frame pointers seem to boost performance in function calls */
		Frame internalInitFrame();
		u64   internalCallMain(const FuncData&);

		// @TODO add some thread data in the future

		void setStatus(vm::api::ExecStatus status);

	public:
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
		 * @brief Force kill the execution of a program (by Supervisor)
		 * Called from the supervisor.
		 */
		void stop();

		void prestart();

		/**
		 * @brief Called on coreThread
		 * coreThread is `main` exec thread
		 */
		void run(const Code&);

		// Given lock cannot be a lock on external_api_mutex
		// If you have access to external_api_mutex, implement this yourself.
		template<class Condition>
		void waitUntilNotPausedAndCondition(std::unique_lock<std::mutex>& lock, Condition x) {
			pause_cv.wait(lock, [this, &x] {
				bool b1 = !isPaused();
				bool b2 = x();
				return b1 && b2;
			});
		}

		void notifyPaused();

		bool isPaused();
		bool isAlive();

		template<class... DynamicServices>
		friend class ServiceManagerDef;

		friend class OpFuns;
	};
}
