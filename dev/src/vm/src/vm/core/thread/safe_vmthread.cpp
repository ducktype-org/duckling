#include "safe_vmthread.hpp"

#include "kill_process_exception.hpp"
#include "opcode_functions/opcodes_functions.hpp"
#include "opcode_functions/opcodes_functions_utils.hpp"

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/misc/int_conv.hpp>
#include <base/types/ints.hpp>

#include <logger/logger.hpp>
#include <string_id/string_id.hpp>

#include <vm/api/data/response.hpp>
#include <vm/api/data/status.hpp>
#include <vm/core/process/concurrency/gil.hpp>
#include <vm/core/process/exceptions.hpp>
#include <vm/core/process/memory/pointer.hpp>
#include <vm/core/process/safe_vmprocess.hpp>
#include <vm/core/process/type_metadata/definitions.hpp>
#include <vm/core/process/type_metadata/type.hpp>
#include <vm/core/thread/low_program/opcodes.hpp>
#include <vm/module_flags/module_flags.hpp>

#include <string>
#include <vector>

namespace vm {
#define MAKE_BYTECODE_INSTRUCTION(OPCODE_NAME, ARG_0, ARG_1) \
	makeLowInstruction(low::MicroOpcode::OPCODE_NAME, ARG_0, ARG_1)

	SafeVMThread::SafeVMThread(api::ThreadID thread_id, SafeVMProcess& process):
		  IVMThread(thread_id, process),
		  runtime_data(process.getMemory().initializeFrameStack()),
		  safe_process(process),
		  process_memory(process.getMemory()),
		  process_program(process.getLoadedProgram()) {}

	/**
	 * @brief Tail call written function that handles the execution pause request.
	 */
	RETURN_TYPE OpFuns::handle_execution_break(OPFUN_ARGS) {
		{
			save_execution_state(instr, local_stack, frame, thread);

			thread.breakActiveExecution();

			// Restore current registers and flow, because
			// they could be changed when doing "step by step" execution.
			frame       = thread.runtime_data.frame_stack_current;
			instr       = frame->instr;
			local_stack = frame->local_stack;
		}
		OPFUN_CONT(1);
	}

	/**
	 * @brief Saves all current execution state in thread memory.
	 *
	 * The opcodes functions in tail call mode passes some state values in the registers
	 * (in the function arguments). This functions saves them from the registers to the
	 * current frame.
	 */
	RETURN_TYPE OpFuns::save_execution_state(OPFUN_ARGS) {
		{
			// Save current registers and flow.
			frame->instr                            = instr;
			frame->local_stack                      = local_stack;
			thread.runtime_data.frame_stack_current = frame;
		}
	}

	/**
	 * @brief Main debug function that executes one step of the program.
	 */
	void SafeVMThread::executeOneStep() {
		Frame*     frame       = runtime_data.frame_stack_current;
		std::byte* local_stack = frame->local_stack;
		auto*      instr       = frame->instr;

		auto opcode = std::to_underlying(getInstructionOpcode(*instr));

		// Execute the instruction by calling the debug opcode function.
		OpFuns::DEBUG_OPFUNS.at(opcode)(instr, local_stack, frame, *this);

		runtime_data.frame_stack_current = frame;
		frame->local_stack               = local_stack;
		frame->instr                     = instr;
	}

	/**
	 * @brief Creates a dynamically generated (meaning it's generated at the moment of a program
	 * invocation) start function for a specified function. For now, mainly used by the REPL mode.
	 *
	 * @note For more detailed explanation go to `createProgramStartFunction`.
	 */
	low::LowFuncData SafeVMThread::createStartFunctionFor(
		const low::LowFuncData& func, const FunctionRunArguments& func_args
	) const {
		if (func_args.size() != func.parameters.size()) {
			throw exceptions::VMRuntimeException(base::strConcat(
				"Function '",
				func.name.str(),
				"' expects ",
				func.parameters.size(),
				" arguments, but ",
				func_args.size(),
				" were provided."
			));
		}

		low::LowFuncData start_function{ .name              = base::StrID("vm_start_function"),
			                             .bc                = {},
			                             .local_stack_size  = 0,
			                             .local_block_count = 0,
			                             .arg_size          = 0,
			                             .ret_size          = func.result_type->getSize().asInt(),
			                             .parameters        = {},
			                             .result_type       = func.result_type };

		u64       result_type_id     = func.result_type->getID().asInt();
		const u64 called_function_id = process_program->getFunctions().idOf(func.name).value();

		// Initialize an exit code/return value spot. In case of non-void functions the exit_code is
		// the return value of the function. Void functions always return with the exit_code = 0.
		start_function.bc.push_back(MAKE_BYTECODE_INSTRUCTION(init_blany_type, 0, result_type_id));

		start_function.local_stack_size += func.result_type->getSize().asInt();

		for (u64 i = 0; i < func_args.size(); i++) {
			const auto& arg_value = func_args[i];
			auto        arg_type  = func.parameters[i];

			if (arg_value->getPID() != safe_process.getPID()) {
				throw exceptions::VMRuntimeException(
					base::strConcat("VMValue for argument ", i, " comes from a different process")
				);
			}

			if (arg_value->type != arg_type) {
				throw exceptions::VMRuntimeException(base::strConcat(
					"Type mismatch for argument ",
					i,
					" of function '",
					func.name.str(),
					"': expected ",
					arg_type->getName().str(),
					", got ",
					arg_value->type->getName().str()
				));
			}

			start_function.bc.push_back(
				MAKE_BYTECODE_INSTRUCTION(initFromVmValue, std::bit_cast<u64>(arg_value.get()), 0)
			);
			start_function.local_stack_size += arg_type->getSize().asInt();
			start_function.parameters.push_back(arg_value->type);
			start_function.arg_size += arg_value->type->getSize().asInt();
		}


		start_function.bc.insert(
			start_function.bc.end(),
			{
				MAKE_BYTECODE_INSTRUCTION(stepGil, 0, 0),  // We need to acquire GIL
				MAKE_BYTECODE_INSTRUCTION(call_func, called_function_id, 0),
				// @note: Only one block is left on the stack in this place, so there is no need for
		        // any deinits. It's being deinitialized by the thread after obtaining the return
		        // value/exit_code.
				MAKE_BYTECODE_INSTRUCTION(exit, 0, 0),
			}
		);
		return start_function;
	}

	/**
	 * @brief Creates a dynamically generated (meaning it's generated at the moment of a program
	 * invocation) start function for a program.
	 *
	 * Just like in libc, the start function pushes the program arguments on to the stack and
	 * performs the call to main. After the main returns, it deinitializes the argv memory and
	 * exits, leaving one block on the block stack, which contains the return value of the program.
	 *
	 * @note This is done in VMThread, since it depends on the arguments passed during the call
	 * which may vary from call to call and creating a generic start function using builders in the
	 * loading phase is not possible. This also results in the need to create the function in the
	 * micro-bytecode right away.
	 */
	low::LowFuncData SafeVMThread::createProgramStartFunction(
		const low::LowFuncData& func, const ProgramRunArguments& args
	) const {
		// Types
		// @note: All the following are guaranteed to exist or their existence was checked
		// during code loading.

		auto        main_return_type = func.result_type;
		const auto& types            = process_program->getTypes();
		auto        argv_type        = types.at(base::StrID("argv"));
		auto        argv_ptr_type    = types.at(base::StrID("ptr_argv"));
		auto        i64_type         = types.at(base::StrID("i64"));
		auto        str_type         = types.at(base::StrID("string"));
		auto        str_ptr_type     = types.at(base::StrID("ptr_string"));
		auto        byte_type        = types.at(base::StrID("byte"));

		low::LowFuncData start_function{ .name              = base::StrID("vm_start_function"),
			                             .bc                = {},
			                             .local_stack_size  = 72,
			                             .local_block_count = 7,
			                             .arg_size          = 0,
			                             .ret_size          = main_return_type->getSize().asInt(),
			                             .parameters        = {},
			                             .result_type       = func.result_type };

		// TypeIDs to pass to opcodes.
		u64 argv_type_id     = argv_type->getID().asInt();
		u64 argv_ptr_type_id = argv_ptr_type->getID().asInt();
		u64 i64_type_id      = i64_type->getID().asInt();
		u64 str_type_id      = str_type->getID().asInt();
		u64 str_ptr_type_id  = str_ptr_type->getID().asInt();
		u64 byte_type_id     = byte_type->getID().asInt();

		const u64  called_function_id = process_program->getFunctions().idOf(func.name).value();
		const bool main_has_args      = !func.parameters.empty();

		// Initialize the needed data first - argc and argv dynamic table.
		// Note that `argv` and `argc` are always initialized even if `main` takes no arguments.
		// This is for the offsets to not get changed when generating the start function.
		start_function.bc.insert(
			start_function.bc.end(),
			{
				// Program return value is fixes to return `i64`.
				MAKE_BYTECODE_INSTRUCTION(
					init_blany_type, 0, i64_type_id
				),  // stack [0, 8), block idx 0 program ret_val
				MAKE_BYTECODE_INSTRUCTION(
					init_blany_type, 1, argv_ptr_type_id
				),  // stack [8, 24) block idx 1 *argv_internal
				MAKE_BYTECODE_INSTRUCTION(
					init_blany_type, 2, i64_type_id
				),  // stack  [24, 32) block idx 2 argc_internal
				MAKE_BYTECODE_INSTRUCTION(
					init_blany_type, 3, i64_type_id
				),  // stack [32, 40) block idx 3 ix
				MAKE_BYTECODE_INSTRUCTION(
					mov_l64_imm, 24, args.size()
				),  // argc_internal := args.size()
				MAKE_BYTECODE_INSTRUCTION(
					dynTableReAlloc_lptr_type, 8, argv_type_id
				),  // alloc *argv_internal
				MAKE_BYTECODE_INSTRUCTION(ext_l64, 24, 0),
			}
		);

		// Now fill in the argv table.
		if (main_has_args) {
			usize argv_index = 0;
		for (const auto& arg: args) {
				start_function.bc.insert(
					start_function.bc.end(),
					{
						MAKE_BYTECODE_INSTRUCTION(
							init_blany_type, 4, str_ptr_type_id
						),  // stack [40, 56) block idx 4 ptr_tmp_store
						MAKE_BYTECODE_INSTRUCTION(
							init_blany_type, 5, byte_type_id
						),  // stack [56, 57) block idx 5 char_tmp_store
						MAKE_BYTECODE_INSTRUCTION(
							mov_l64_imm, 24, arg.size() + 1
						),  // argc_internal := arg.size() + 1 (for the \0 character)
						MAKE_BYTECODE_INSTRUCTION(
							dynTableReAlloc_lptr_type, 40, str_type_id
						),                                              // alloc ptr_tmp_store
						MAKE_BYTECODE_INSTRUCTION(ext_l64, 24, 0),
						MAKE_BYTECODE_INSTRUCTION(mov_l64_imm, 32, 0),  // ix := 0
					}
				);
				for (auto c: arg) {
					start_function.bc.insert(
						start_function.bc.end(),
						{ MAKE_BYTECODE_INSTRUCTION(
							  mov_l8_imm, 56, static_cast<u64>(c)
						  ),  // char_tmp_store := c
					      MAKE_BYTECODE_INSTRUCTION(
							  dynTableStore_lptr_blany, 40, 5
						  ),  // ptr_tmp_store[ix] := char_tmp_store
					      MAKE_BYTECODE_INSTRUCTION(ext_l64, 32, 0),
					      MAKE_BYTECODE_INSTRUCTION(add_l64_imm, 32, 1) }
					);
				}
				start_function.bc.insert(
					start_function.bc.end(),
					{
						// At this point ix == arg.size().
						MAKE_BYTECODE_INSTRUCTION(mov_l8_imm, 56, 0),  // char_tmp_store := \0
						MAKE_BYTECODE_INSTRUCTION(
							dynTableStore_lptr_blany, 40, 5
						),  // ptr_tmp_store[ix] := char_tmp_store
						MAKE_BYTECODE_INSTRUCTION(ext_l64, 32, 0),
						MAKE_BYTECODE_INSTRUCTION(
							mov_l64_imm, 32, base::safeIntConv<u64>(argv_index)
						),  // ix := argv_index
						MAKE_BYTECODE_INSTRUCTION(
							dynTableStore_lptr_blany, 8, 4
						),  // argv_internal[ix] := ptr_tmp_store
						MAKE_BYTECODE_INSTRUCTION(ext_l64, 32, 0),
						MAKE_BYTECODE_INSTRUCTION(deinit, 0, 0),  // deinit char_tmp_store
						MAKE_BYTECODE_INSTRUCTION(deinit, 0, 0),  // deinit ptr_tmp_store
					}
				);
				++argv_index;
			}
		}


		// Now actually prepare to call 'main'.
		start_function.bc.push_back(
			MAKE_BYTECODE_INSTRUCTION(init_blany_type, 4, i64_type_id)  // [40, 48) main ret_val
		);

		// Pass the command line arguments only if main signature specifies it.
		if (main_has_args) {
			start_function.bc.insert(
				start_function.bc.end(),
				{
					MAKE_BYTECODE_INSTRUCTION(init_blany_type, 5, i64_type_id),  // [48, 56) argc
					MAKE_BYTECODE_INSTRUCTION(
						init_blany_type, 6, argv_ptr_type_id
					),                                                        // [56, 72) *argv
					MAKE_BYTECODE_INSTRUCTION(mov_l64_imm, 48, args.size()),  // argc := args.size()
					MAKE_BYTECODE_INSTRUCTION(mov_lptr_lptr, 56, 8),  // argv := argv_internal
				}
			);
		}

		start_function.bc.insert(
			start_function.bc.end(),
			{
				MAKE_BYTECODE_INSTRUCTION(stepGil, 0, 0),  // We need to acquire GIL
				MAKE_BYTECODE_INSTRUCTION(call_func, called_function_id, 0),  // call main
				MAKE_BYTECODE_INSTRUCTION(mov_l64_l64, 0, 40),  // ret_val := main_ret_val
				MAKE_BYTECODE_INSTRUCTION(mov_l64_imm, 32, 0),  // ix := 0
				MAKE_BYTECODE_INSTRUCTION(
					init_blany_type, 5, str_ptr_type_id
				),  // [48, 64) ptr_tmp_store
			}
		);

		// After 'main' returned, free all the allocated strings in the argv table.
		for ([[maybe_unused]] const auto& arg: args) {
			start_function.bc.insert(
				start_function.bc.end(),
				{
					MAKE_BYTECODE_INSTRUCTION(
						dynTableLoad_blany_lptr, 5, 8
					),  // ptr_tmp_store := argv_internal[ix]
					MAKE_BYTECODE_INSTRUCTION(ext_l64, 32, 0),
					MAKE_BYTECODE_INSTRUCTION(free_lptr, 48, 0),    // free ptr_tmp_store
					MAKE_BYTECODE_INSTRUCTION(add_l64_imm, 32, 1),  // ++ix
				}
			);
		}

		// Lastly, free all the data allocated by the start function.
		start_function.bc.insert(
			start_function.bc.end(),
			{
				MAKE_BYTECODE_INSTRUCTION(free_lptr, 8, 0),  // free *argv_internal
				MAKE_BYTECODE_INSTRUCTION(deinit, 0, 0),     // deinit ptr_tmp_store
				MAKE_BYTECODE_INSTRUCTION(deinit, 0, 0),     // deinit main_ret_val
				MAKE_BYTECODE_INSTRUCTION(deinit, 0, 0),     // deinit ix
				MAKE_BYTECODE_INSTRUCTION(deinit, 0, 0),     // deinit argc_internal
				MAKE_BYTECODE_INSTRUCTION(deinit, 0, 0),     // deinit *argv_internal
				// At this point only the start function return value (which is the program exit
		        // code) remains on the stack.
				MAKE_BYTECODE_INSTRUCTION(exit, 0, 0),
			}
		);
		return start_function;
	}

#if defined(__clang__)
// @TODO: suppress code deduplication in Clang
#elif defined(__GNUG__)
	#pragma GCC push_options
	#pragma GCC optimize("-fno-crossjumping")
#endif
	// NOLINTBEGIN(cppcoreguidelines-avoid-goto)
	// NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
	Ref<VmValue> SafeVMThread::executeFunction(
		const low::LowFuncData& start_function, const low::LowFuncData& func
	) {
		acquireGil();
		// Frame of the called function.
		Frame*     frame       = runtime_data.frame_stack_base;
		std::byte* local_stack = runtime_data.local_stack_base;

		frame->current_function           = &start_function;
		frame->local_block_ref_stack_base = runtime_data.block_ref_stack_base;
		frame->local_block_ref_stack_end  = runtime_data.block_ref_stack_base;

		const auto* instr = start_function.bc.data();

#ifdef USE_TAIL_CALLS
		instr->tc_opfun(instr, local_stack, frame, *this);

#elif defined(USE_SWITCH_CASE)
		while (true) {
			switch (static_cast<low::MicroOpcode>(instr->nontc_opcode)) {
	#define HANDLE_MICRO_INSTR(opcode_name)                                                         \
	case low::MicroOpcode::opcode_name: {                                                           \
		vm::OpFuns::op_##opcode_name(instr, local_stack, frame, *this);                             \
		if constexpr (::vm::ENABLE_VM_DETAIL_LOGGING)                                               \
			CORE_DEV_LOG(DVMDetails, "Executed opcode: ", #opcode_name);                            \
		if constexpr (constexpr std::string_view opcode_str = #opcode_name; opcode_str == "exit") { \
			goto End;                                                                               \
		} else {                                                                                    \
			break;                                                                                  \
		}                                                                                           \
	}
	#include <vm/core/thread/low_program/micro_instruction_definitions.hpp>
	#undef HANDLE_MICRO_INSTR

			default: {
				CORE_PANIC("Unknown operator: ", u64(instr->nontc_opcode));
			}
			}
		}
	End:
#endif
		// @note: The return value is the only block left on the block stack.
		CORE_ASSERT(
			frame->local_block_ref_stack_end - frame->local_block_ref_stack_base == 1,
			"After function execution, there should be exactly one block on the block stack."
		);
		auto block         = Ref(frame->local_block_ref_stack_base[0]);
		exit_value_storage = safe_process.createVmValue(func.result_type, Pointer(block, 0));
		process_memory.freeBlockData(block);
		process_memory.decreaseBlockRefcount(block);
		frame->resetFrameData();
		releaseGil();

		return exit_value_storage.value();
	}

	// executeFunction end

	// NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
	// NOLINTEND(cppcoreguidelines-avoid-goto)

#if defined(__clang__)
// @TODO: suppress code deduplication in Clang
#elif defined(__GNUG__)
	#pragma GCC pop_options
#endif

	/**
	 * @brief Starts the execution of a function with a given name and arguments.
	 */
	void SafeVMThread::run(const std::string& func_name, const RunArguments& run_arguments) {
		respondExecutionRequest(api::Running{});

		for (const auto& [global, id, name]: process_program->getGlobals().allData()) {
			// Insert the global data if it hasn't been initialized; then run constructor if present
			if (process_memory.tryInsertGlobalData(id, global->type)
			    && global->ctor_name.has_value()) {
				try {
					const auto& func
						= *process_program->getFunctions().atMaybe(global->ctor_name.value()).value();
					low::LowFuncData start_function = createStartFunctionFor(func, {});
					executeFunction(start_function, func);
				} catch (const KillProcessException& e) {
					respondExecutionRequest(api::ExecutionPanicked{ e.what() });
				}
			}
		}

		try {
			const auto& maybe_func
				= process_program->getFunctions().atMaybe(base::StrID(func_name.data()));
			if (!maybe_func.has_value()) {
				respondExecutionRequest(api::ExecutionPanicked{
					base::strConcat("Called function '", func_name, "' does not exist.") });
				return;
			}
			const auto& func = *maybe_func.value();

			low::LowFuncData start_function = [&]() {
				variant_match(run_arguments) {
					variant_case(ProgramRunArguments, program_run_arguments) {
						return createProgramStartFunction(func, program_run_arguments);
					}
					variant_case(FunctionRunArguments, function_run_data) {
						return createStartFunctionFor(func, function_run_data);
					}
				}
				CORE_UNREACHABLE();
			}();

			const auto exit_value = executeFunction(start_function, func);
			respondExecutionRequest(api::ExecutionCompleted{ exit_value });
		} catch (const KillProcessException& e) {
			respondExecutionRequest(api::ExecutionPanicked{ e.what() });
		}
	}

	void SafeVMThread::execGlobalDestructors() {
		const auto& executing_program = process_program;
		for (const auto& [global, id, name]: executing_program->getGlobals().allData()) {
			if (global->dtor_name.has_value()) {
				try {
					const auto& func = *executing_program->getFunctions()
					                        .atMaybe(base::StrID(global->dtor_name.value()))
					                        .expect(
												"Called function does not exist: "
												+ global->dtor_name.value().str()
											);
					low::LowFuncData start_function = createStartFunctionFor(func, {});
					executeFunction(start_function, func);
				} catch (const KillProcessException& e) {
					respondExecutionRequest(api::ExecutionPanicked{ e.what() });
				}
			}
		}
	}

	std::expected<api::Response, api::ApiError> SafeVMThread::getCurrentPosition() {
		variant_match(getStatus()) {
			variant_case_novalue(api::Paused) {
				auto frame = runtime_data.frame_stack_current;
				auto instr = frame->instr;

				for (const auto& [idx, func]:
				     std::views::enumerate(process_program->getFunctions())) {
					if (func.bc.data() <= instr && instr < func.bc.data() + func.bc.size()) {
						return api::Response(api::response::CodePosition{
							.function_id  = static_cast<u64>(idx),  // Assuming function_id is int
							.instr_number = static_cast<u64>(instr - func.bc.data()) });
					}
				}
			}
			variant_default {
				return std::unexpected(api::ApiError{
					api::OtherError{ "wrong execution status while reading current position" } });
			}
		}
		CORE_UNREACHABLE();
	}

	void SafeVMThread::stepGil() {
		if (has_gil) {
			// Check if you can hold it longer - releasing policy
			// If you can't hold it longer then
			// 1. say
			if (!safe_process.getGIL().shouldRelease()) return;
			// 2. release gil
			releaseGil();
			// 3. yield - to not reacquire instantly
			std::this_thread::yield();
		}
		// Try to acquire GIL
		acquireGil();
	}

	void SafeVMThread::releaseGil() {
		CORE_ASSERT(has_gil, "Cannot release GIL without acquiring it first");
		has_gil = false;
		safe_process.getGIL().release();
	}

	void SafeVMThread::acquireGil() {
		CORE_ASSERT(!has_gil, "Cannot acquire GIL twice");
		safe_process.getGIL().acquire();
		has_gil = true;
	}

	void SafeVMThread::setThreadCtx(std::string str) { thread_ctx = std::move(str); }

	u64 SafeVMThread::getNumberOfCurrentStackFrames() const {
		// +1 because frame_stack_current points to the current frame, not the next free slot.
		return u64(runtime_data.frame_stack_current - runtime_data.frame_stack_base) + 1;
	}

	Frame& SafeVMThread::getStackFrame(u64 frame_index) {
		return runtime_data.frame_stack_base[frame_index];
	}
}
