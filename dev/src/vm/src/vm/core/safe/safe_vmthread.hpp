#pragma once

#include <base/collections/optional.hpp>
#include <base/types/ints.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/response.hpp>
#include <vm/api/data/status.hpp>
#include <vm/core/process/interface_types.hpp>
#include <vm/core/safe/low_program/low_program.hpp>
#include <vm/core/safe/memory/memory.hpp>
#include <vm/core/safe/memory/thread_stack.hpp>
#include <vm/core/thread/ivmthread.hpp>
#include <vm/core/thread/kill_process_exception.hpp>
#include <vm/core/vmvalue/ivmvalue.hpp>

#include <limits>

#ifdef ENABLE_JIT
	#include <vm/core/jit/jit_compiler.hpp>
#endif


#include <expected>

namespace vm {
	// Forward declarations
	namespace builtins {
		class FunctionHandlers;
	}

	class SafeVMProcess;
	class SafeVMValue;

	/**
	 * @brief Frames are on stack, this is the maximum number of frame pointers available.
	 */
	constexpr u64 FRAME_COUNT = ThreadStack::FRAMES_LENGTH;

	/**
	 * @brief Number of fixed and preallocated stack bytes.
	 */
	constexpr u64 STACK_LENGTH = ThreadStack::STACK_LENGTH;

	/**
	 * @brief This structure holds pointers to `frame_stack`, `local_stack`, `slot_stack`
	 * and global buffer vectors for fast access during runtime.
	 *
	 * Note that these pointers are non-owning and just for easier access (less dereferencing)
	 * from the opcode functions.
	 *
	 * `frame_stack` is a vector of frames, that we use like a stack. Top of the stack is saved in
	 * the `frame` argument passed inside opcode functions, which is also the current frame.
	 * `local_stack_reserved` is one continuous block of memory, from which every function gets it's
	 * own chunk. It also behaves like a stack, but can be moved forward by many bytes, so
	 * `local_stack_top` is kept to remember where the top of the stack currently is.
	 */
	struct RuntimeData final {
		Frame* frame_stack_base;     /// Pointer to the first frame from `frame_stack` vector.
		Frame* frame_stack_end;      /// Pointer to the first value not allocated.
		Frame* frame_stack_current;  /// Pointer to the current frame - used only when debugging.

		byte* local_stack_base;      /// Pointer to the start of `local_stack_reserved`.
		byte* local_stack_end;       /// Pointer to the first value not allocated.

		LocalSlot* slot_stack_base;  /// Pointer to the start of the local variable slot stack.
		LocalSlot* slot_stack_end;   /// Pointer to the first value not allocated.

		byte*   global_data_buffer_base;       /// Pointer to the start of global data buffer.
		Block** global_block_ref_buffer_base;  /// Pointer to the start of global block ref buffer.

		RuntimeData(Ref<ThreadStack> stack, GlobalBufferPointersByte global_buffer_pointers):
			  frame_stack_base(stack->getFrameStack()->data()),
			  frame_stack_end(stack->getFrameStack()->data() + stack->getFrameStack()->size()),
			  frame_stack_current(stack->getFrameStack()->data()),
			  local_stack_base(stack->getLocalStack()->data()),
			  local_stack_end(stack->getLocalStack()->data() + stack->getLocalStack()->size()),
			  slot_stack_base(stack->getSlotStack()->data()),
			  slot_stack_end(stack->getSlotStack()->data() + stack->getSlotStack()->size()),
			  global_data_buffer_base(global_buffer_pointers.data_buffer_base),
			  global_block_ref_buffer_base(global_buffer_pointers.blocks_buffer_base) {}
	};

	/**
	 * @brief Safe implementation of the IVMThread interface.
	 */
	class SafeVMThread final: public IVMThread {
	private:
		/**
		 * @brief High-level representation of the synthetic `vm_start_function`, when this thread
		 * has loaded one. Kept here so `LowFuncData::high_func` stays valid for the lowered
		 * function's whole lifetime.
		 */
		base::Optional<code::valid_function::ValidFunction> start_function_high;

		/**
		 * @brief Lowered bytecode of the synthetic `vm_start_function`, when this thread has loaded
		 * one. The optionals mark whether a start function is currently loaded.
		 */
		base::Optional<low::LowFuncData> start_function_low;

		RuntimeData runtime_data;

		/**
		 * @brief Link to parent process.
		 */
		SafeVMProcess& safe_process;

		/**
		 * @brief Parent process'es memory.
		 */
		Memory& process_memory;
		/**
		 * @brief Parent process'es program.
		 */
		CRef<vm::low::ILowVMProgram> process_program;

		/**
		 * @brief True if a thread currently occupies GIL.
		 */
		bool has_gil = false;

		/**
		 * @brief Stores exit value of the last ran function. ExecutionCompleted exec status can
		 * store a reference to this object.
		 */
		base::Optional<std::vector<Ref<SafeVMValue>>> exit_value_storage{};

		/**
		 * @brief Thread context - currently just the name of the function that will be used in
		 * builtin spawn thread.
		 */
		std::string thread_ctx;

		/**
		 * @brief RAII object guaranteeing the release of the GIL lock.
		 */
		struct ScopedGilGuard final {
			SafeVMThread& thread;
			explicit ScopedGilGuard(SafeVMThread& t);
			ScopedGilGuard(const ScopedGilGuard&)            = delete;
			ScopedGilGuard& operator=(const ScopedGilGuard&) = delete;
			~ScopedGilGuard();
		};

		/**
		 * @brief RAII guard for a blocking wait (IO, mutex, a condition variable):
		 * reports the thread as sleeping and releases the GIL on construction, then reacquires the
		 * GIL and reports the thread as running again when the scope ends.
		 *
		 * @note The thread must be `Running` when the guard is created.
		 */
		struct ScopedBlockingWait final {
			SafeVMThread& thread;
			explicit ScopedBlockingWait(SafeVMThread& t);
			ScopedBlockingWait(const ScopedBlockingWait&)            = delete;
			ScopedBlockingWait& operator=(const ScopedBlockingWait&) = delete;
			~ScopedBlockingWait();
		};

		/**
		 * @brief Creates a list of instructions, which initialize the argv table and populate it
		 * with given command line `args`, push the argc and *argv blocks onto mains local stack,
		 * perform the call and deinitialize the argv table when main returns.
		 */
		[[nodiscard]] code::Function createProgramStartFunction(
			const low::LowFuncData& func, const ProgramRunArguments& args
		) const;

		/**
		 * @brief Creates a high-level function, which pushes the passed `func_args` onto the local
		 * stack and performs a call to `func`. It is validated and lowered like any other
		 * function before it can be executed.
		 */
		[[nodiscard]] code::Function createStartFunctionFor(
			const low::LowFuncData& func, const FunctionRunArguments& func_args
		) const;

		/**
		 * @brief Validates the high-level `vm_start_function` and lowers it to low bytecode,
		 * loading both into `start_function_high` / `start_function_low`.
		 * @note The high representation is kept in `start_function_high`, so the address kept in
		 * `LowFuncData::high_func` stays valid for the lowered function's whole lifetime.
		 */
		void compileAndLoadStartFunction(const code::Function& start_function);

		/**
		 * @brief This is the primary function to call to start execution on the VM. It runs the
		 * loaded `start_function_low`, which calls @p func.
		 * @param func - the function the loaded start function was built for; its result signature
		 * is what the exit values are extracted by.
		 * @return Mutable references to the SafeVMValues returned by the program.
		 */
		std::vector<Ref<SafeVMValue>> executeLoadedFunction(const low::LowFuncData& func);

		void execGlobalDestructors() override;

		/**
		 * @brief Opcode of the instruction the thread would execute next. `breakpoint`
		 * gets resolved to the instruction it replaced.
		 */
		[[nodiscard]] low::MicroOpcode getCurrentOpcode() const;

	protected:
		void executeOneStep() override;

		[[nodiscard]] bool isAtExecutionEnd() const override;

	public:
		SafeVMThread(api::ThreadID thread_id, SafeVMProcess& process);

		/**
		 * @brief Run a single function with given parameters.
		 */
		void run(const std::string& func_name, const RunArguments& run_arguments) override;

		std::expected<low::LowCodePosition, api::ApiError> getCurrentPosition(
			base::Optional<usize> frame_idx = std::nullopt
		);

		friend class SafeVMProcess;
		friend class OpFuns;
		friend class builtins::FunctionHandlers;

		/**
		 * @brief Runs GIL logic. Should be called periodically to allow GIL release.
		 *
		 * If thread does not have GIL then acquires it.
		 * If thread already has GIL:
		 *   -> if the GIL should be released, releases it
		 *   -> otherwise does nothing.
		 */
		void stepGil();

		/**
		 * @brief Releases the GIL if it's taken.
		 */
		void releaseGilIfHeld() override;

		/**
		 * @brief Reacquires the GIL if it's not taken already.
		 */
		void acquireGilIfNotHeld() override;

		/**
		 * @brief Releases GIL.
		 */
		void releaseGil();

		/**
		 * @brief Acquires GIL.
		 */
		void acquireGil();

		/**
		 * @brief Sets name of the function that will be used in builtin spawn thread.
		 */
		void setThreadCtx(std::string);

		/**
		 * @brief Gets name of the function that will be used in builtin spawn thread.
		 */
		[[nodiscard]] const std::string& getThreadCtx() const { return thread_ctx; }

		/**
		 * @brief Checks if the function with the specified ID can be called from the runtime.
		 * @note By correctness of the compiler, this only checks if the function is not the start
		 * function.
		 */
		static bool isCallableFunctionID(usize id);

		[[nodiscard]] u64 getNumberOfCurrentStackFrames() const override;

		Frame& getStackFrame(u64 frame_index);

		/**
		 * @brief Update the pointers to the global data buffer and global blocks buffer.
		 * For now only the VMProcess calls this function after the global data memory is
		 * reallocated and the pointers change.
		 */
		void updateGlobalDataBufferPointers(GlobalBufferPointersByte global_buffer_pointers);
	};

	/**
	 * @brief This is a low level function to run the interpreter, until the `exit` instruction
	 * appears. Probably shouldn't be called directly, look into `executeFunction` first.
	 *
	 * @param instr - the first instruction that to be executed
	 */
	void runInterpreter(
		const MicroInstruction* instr, byte*& local_stack, Frame*& frame, SafeVMThread& thread
	);
}
