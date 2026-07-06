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
	class SafeVmValue;

	/**
	 * @brief Frames are on stack, this is the maximum number of frame pointers available.
	 */
	constexpr u64 FRAME_COUNT = ThreadStack::FRAMES_LENGTH;

	/**
	 * @brief Number of fixed and preallocated stack bytes.
	 */
	constexpr u64 STACK_LENGTH = ThreadStack::STACK_LENGTH;

	/**
	 * @brief This structure holds pointers to `frame_stack`, `local_stack`, `block_ref_stack`
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
	struct RuntimeData {
		Frame* frame_stack_base;       /// Pointer to the first frame from `frame_stack` vector.
		Frame* frame_stack_end;        /// Pointer to the first value not allocated.
		Frame* frame_stack_current;    /// Pointer to the current frame - used only when debugging.

		std::byte* local_stack_base;   /// Pointer to the start of `local_stack_reserved`.
		std::byte* local_stack_end;    /// Pointer to the first value not allocated.

		Block** block_ref_stack_base;  /// Pointer to the start of `block_ref_stack_reserved`.
		Block** block_ref_stack_end;   /// Pointer to the first value not allocated.

		std::byte* global_data_buffer_base;    /// Pointer to the start of global data buffer.
		Block** global_block_ref_buffer_base;  /// Pointer to the start of global block ref buffer.

		RuntimeData(Ref<ThreadStack> stack, GlobalBufferPointers global_buffer_pointers):
			  frame_stack_base(stack->getFrameStack()->data()),
			  frame_stack_end(stack->getFrameStack()->data() + stack->getFrameStack()->size()),
			  frame_stack_current(stack->getFrameStack()->data()),
			  local_stack_base(stack->getLocalStack()->data()),
			  local_stack_end(stack->getLocalStack()->data() + stack->getLocalStack()->size()),
			  block_ref_stack_base(stack->getBlockRefStack()->data()),
			  block_ref_stack_end(
				  stack->getBlockRefStack()->data() + stack->getBlockRefStack()->size()
			  ),
			  global_data_buffer_base(global_buffer_pointers.data_buffer_base),
			  global_block_ref_buffer_base(global_buffer_pointers.blocks_buffer_base) {}
	};

	/**
	 * @brief Safe implementation of the IVMThread interface.
	 */
	class SafeVMThread final: public IVMThread {
	private:
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
		 * @brief Mock ID of the VM program start function.
		 * This has to be declared explicitly because the start function object is never
		 * inserted into the `functions` collection, so it doesn't have a real ID; this ID should
		 * never be assigned to a real function.
		 */
		static constexpr usize START_FUNCTION_ID = std::numeric_limits<usize>::max();

		/**
		 * @brief Stores exit value of the last ran function. ExecutionCompleted exec status can
		 * store a reference to this object.
		 */
		base::Optional<std::vector<Ref<SafeVmValue>>> exit_value_storage{};

		/**
		 * @brief Thread context - currently just the name of the function that will be used in
		 * builtin spawn thread.
		 */
		std::string thread_ctx;

#ifdef ENABLE_JIT
		jit::JitData jit_data;
#endif

		/**
		 * @brief RAII object guaranteeing the release of the GIL lock.
		 */
		struct ScopedGilGuard {
			SafeVMThread& thread;
			ScopedGilGuard(SafeVMThread& t);
			~ScopedGilGuard();
		};

		/**
		 * @brief Creates a list of instructions, which initialize the argv table and populate it
		 * with given command line `args`, push the argc and *argv blocks onto mains local stack,
		 * perform the call and deinitialize the argv table when main returns.
		 */
		[[nodiscard]] low::LowFuncData createProgramStartFunction(
			const low::LowFuncData& func, const ProgramRunArguments& args
		) const;

		/**
		 * @brief Creates a list of instructions, which push the passed `func_args` onto the local
		 * stack and perform a call to `func`.
		 */
		[[nodiscard]] low::LowFuncData createStartFunctionFor(
			const low::LowFuncData& func, const FunctionRunArguments& func_args
		) const;

		/**
		 * @brief This is the primary function to call to start execution on the VM.
		 * It calls both the main function when running the program and single functions called by
		 * the `runFunction` endpoint. It starts the execution beginning with the first instruction
		 * in the start_function bytecode vector.
		 * @param start_function - the code of the start function.
		 * @param func - the function to execute.
		 * @return Mutable references to the SafeVmValues returned by the program
		 */
		std::vector<Ref<SafeVmValue>> executeFunction(
			const low::LowFuncData& start_function, const low::LowFuncData& func
		);

		void execGlobalDestructors() override;

		void handleKillProcessException(const KillProcessException& e);

	protected:
		void executeOneStep() override;

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
		void updateGlobalDataBufferPointers(GlobalBufferPointers global_buffer_pointers);
	};

	/**
	 * @brief This is a low level function to run the interpreter, until the `exit` instruction
	 * appears. Probably shouldn't be called directly, look into `executeFunction` first.
	 *
	 * @param instr - the first instruction that to be executed
	 */
	void runInterpreter(
		const MicroInstruction* instr, std::byte*& local_stack, Frame*& frame, SafeVMThread& thread
	);
}
