#include "safe_vmthread.hpp"

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
#include <vm/core/safe/concurrency/gil.hpp>
#include <vm/core/safe/exceptions.hpp>
#include <vm/core/safe/low_program/cfg/cf_graph.hpp>
#include <vm/core/safe/low_program/low_program.hpp>
#include <vm/core/safe/low_program/opcodes.hpp>
#include <vm/core/safe/memory/pointer.hpp>
#include <vm/core/safe/safe_vmprocess.hpp>
#include <vm/core/safe/type_metadata/definitions.hpp>
#include <vm/core/safe/type_metadata/type.hpp>
#include <vm/core/safe/vmvalue/safe_vmvalue.hpp>
#include <vm/core/thread/thread_state.hpp>
#include <vm/module_flags/module_flags.hpp>
#include <vm/utils/interpret.hpp>

#include <ranges>
#include <string>
#include <vector>

namespace vm {
	namespace ts = thread_state;
	namespace te = thread_event;

#define MAKE_BYTECODE_INSTRUCTION(OPCODE_NAME, ARG_0, ARG_1) \
	makeLowInstruction(low::MicroOpcode::OPCODE_NAME, ARG_0, ARG_1)

	SafeVMThread::SafeVMThread(api::ThreadID thread_id, SafeVMProcess& process):
		  IVMThread(thread_id, process),
		  runtime_data(
			  process.getMemory().initializeFrameStack(), process.getMemory().getGlobalDataMemory()
		  ),
		  safe_process(process),
		  process_memory(process.getMemory()),
		  process_program(process.getLoadedProgram()) {}

	/**
	 * @brief Tail call written function that handles the execution pause request.
	 * @details Assumes that the instruction in the frame is to be executed before AND after running
	 * this function.
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
		OPFUN_CONT(0);
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
		auto*      instr       = frame->instr;
		std::byte* local_stack = frame->local_stack;

		low::MicroOpcode opcode = getInstructionOpcode(*instr);
		if (opcode == low::MicroOpcode::breakpoint) {
			const auto* program_copy
				= dynamic_cast<const low::LowVMProgramCopy*>(process_program.get());
			CORE_ASSERT(program_copy, "Breakpoints should be only in LowVMProgramCopy.");

			auto original_instr
				= program_copy->getOriginalProgram()
			          ->getFunctions()
			          .at(frame->current_function->name)
			          ->bc[static_cast<size_t>(frame->instr - &frame->current_function->bc[0])];

			opcode = getInstructionOpcode(original_instr);
		}

		// Execute the instruction by calling the debug opcode function.
		OpFuns::DEBUG_OPFUNS.at(std::to_underlying(opcode))(instr, local_stack, frame, *this);

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
		low::LowFuncData start_function{
			.name = base::StrID("vm_start_function"),
			.id   = START_FUNCTION_ID,
#ifdef ENABLE_JIT
			.cfg
			= low::cf::ControlFlowGraph(),  // This is okay because we never JIT the start function.
#endif
			.bc                  = {},
			.local_stack_size    = 0,
			.local_block_count   = func.result_types.size() + func.parameters.size(),
			.arg_size            = 0,
			.ret_size            = func.ret_size,
			.parameters          = {},
			.result_types        = func.result_types,
			.instruction_mapping = {}
		};

		const u64 called_function_id = process_program->getFunctions().idOf(func.name).value();

		for (auto [idx, res]: std::views::enumerate(func.result_types)) {
			// Initialize an exit code/return value spot. In case of non-void functions the
			// exit_code is the return value of the function. Void functions always return with the
			// exit_code = 0.
			start_function.bc.push_back(
				MAKE_BYTECODE_INSTRUCTION(init_bany_type, (u64) idx, safeReadObjectBytes<u64>(res))
			);
		}

		start_function.local_stack_size += func.ret_size;

		// Argument validity was already checked when validating the API call.
		for (const auto& [i, arg_value]: std::views::zip(std::views::iota(0u), func_args)) {
			const auto& arg_type = func.parameters[i];

			start_function.bc.push_back(MAKE_BYTECODE_INSTRUCTION(
				initFromVMValue, std::bit_cast<u64>(dynamic_cast<const SafeVMValue*>(&*arg_value)), 0
			));
			start_function.local_stack_size += arg_type->getSize().asInt();
			start_function.parameters.push_back(arg_type);
			start_function.arg_size += arg_type->getSize().asInt();
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

		auto        main_return_type = func.result_types;
		const auto& types            = process_program->getTypes();
		auto        argv_type        = types.at(base::StrID("argv"));
		auto        argv_ptr_type    = types.at(base::StrID("ptr_argv"));
		auto        i64_type         = types.at(base::StrID("i64"));
		auto        str_type         = types.at(base::StrID("string"));
		auto        str_ptr_type     = types.at(base::StrID("ptr_string"));
		auto        byte_type        = types.at(base::StrID("byte"));

		low::LowFuncData start_function{
			.name = base::StrID("vm_start_function"),
			.id   = START_FUNCTION_ID,
#ifdef ENABLE_JIT
			.cfg
			= low::cf::ControlFlowGraph(),  // This is okay because we never JIT the start function.
#endif
			.bc                  = {},
			.local_stack_size    = 72,
			.local_block_count   = 7,
			.arg_size            = 0,
			.ret_size            = func.ret_size,
			.parameters          = {},
			.result_types        = func.result_types,
			.instruction_mapping = {}
		};

		// TypeIDs to pass to opcodes.
		u64 argv_type_arg     = safeReadObjectBytes<u64>(argv_type);
		u64 argv_ptr_type_arg = safeReadObjectBytes<u64>(argv_ptr_type);
		u64 i64_type_arg      = safeReadObjectBytes<u64>(i64_type);
		u64 str_type_arg      = safeReadObjectBytes<u64>(str_type);
		u64 str_ptr_type_arg  = safeReadObjectBytes<u64>(str_ptr_type);
		u64 byte_type_arg     = safeReadObjectBytes<u64>(byte_type);

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
					init_bany_type, 0, i64_type_arg
				),  // stack [0, 8), block idx 0 program ret_val
				MAKE_BYTECODE_INSTRUCTION(
					init_bany_type, 1, argv_ptr_type_arg
				),  // stack [8, 24) block idx 1 *argv_internal
				MAKE_BYTECODE_INSTRUCTION(
					init_bany_type, 2, i64_type_arg
				),  // stack  [24, 32) block idx 2 argc_internal
				MAKE_BYTECODE_INSTRUCTION(
					init_bany_type, 3, i64_type_arg
				),  // stack [32, 40) block idx 3 ix
				MAKE_BYTECODE_INSTRUCTION(
					mov_p64_imm, 24, args.size()
				),  // argc_internal := args.size()
				MAKE_BYTECODE_INSTRUCTION(
					dynTableReAlloc_pptr_type, 8, argv_type_arg
				),  // alloc *argv_internal
				MAKE_BYTECODE_INSTRUCTION(ext_p64, 24, 0),
			}
		);

		// Now fill in the argv table.
		if (main_has_args) {
			for (const auto& [argv_index, arg]:
			     std::views::zip(std::ranges::views::iota(0u), args)) {
				start_function.bc.insert(
					start_function.bc.end(),
					{
						MAKE_BYTECODE_INSTRUCTION(
							init_bany_type, 4, str_ptr_type_arg
						),  // stack [40, 56) block idx 4 ptr_tmp_store
						MAKE_BYTECODE_INSTRUCTION(
							init_bany_type, 5, byte_type_arg
						),  // stack [56, 57) block idx 5 char_tmp_store
						MAKE_BYTECODE_INSTRUCTION(
							mov_p64_imm, 24, arg.size() + 1
						),  // argc_internal := arg.size() + 1 (for the \0 character)
						MAKE_BYTECODE_INSTRUCTION(
							dynTableReAlloc_pptr_type, 40, str_type_arg
						),                                              // alloc ptr_tmp_store
						MAKE_BYTECODE_INSTRUCTION(ext_p64, 24, 0),
						MAKE_BYTECODE_INSTRUCTION(mov_p64_imm, 32, 0),  // ix := 0
					}
				);
				for (auto c: arg) {
					start_function.bc.insert(
						start_function.bc.end(),
						{ MAKE_BYTECODE_INSTRUCTION(
							  mov_p8_imm, 56, static_cast<u64>(c)
						  ),  // char_tmp_store := c
					      MAKE_BYTECODE_INSTRUCTION(
							  anyArrayStore_pptr_bany, 40, 5
						  ),  // ptr_tmp_store[ix] := char_tmp_store
					      MAKE_BYTECODE_INSTRUCTION(ext_p64_type, 32, byte_type_arg),
					      MAKE_BYTECODE_INSTRUCTION(add_p64_imm, 32, 1) }
					);
				}
				start_function.bc.insert(
					start_function.bc.end(),
					{
						// At this point ix == arg.size().
						MAKE_BYTECODE_INSTRUCTION(mov_p8_imm, 56, 0),  // char_tmp_store := \0
						MAKE_BYTECODE_INSTRUCTION(
							anyArrayStore_pptr_bany, 40, 5
						),  // ptr_tmp_store[ix] := char_tmp_store
						MAKE_BYTECODE_INSTRUCTION(ext_p64_type, 32, byte_type_arg),
						MAKE_BYTECODE_INSTRUCTION(mov_p64_imm, 32, argv_index),  // ix := argv_index
						MAKE_BYTECODE_INSTRUCTION(
							anyArrayStore_pptr_bany, 8, 4
						),  // argv_internal[ix] := ptr_tmp_store
						MAKE_BYTECODE_INSTRUCTION(ext_p64_type, 32, str_ptr_type_arg),
						MAKE_BYTECODE_INSTRUCTION(deinit, 0, 0),  // deinit char_tmp_store
						MAKE_BYTECODE_INSTRUCTION(deinit, 0, 0),  // deinit ptr_tmp_store
					}
				);
			}
		}


		// Now actually prepare to call 'main'.
		start_function.bc.push_back(
			MAKE_BYTECODE_INSTRUCTION(init_bany_type, 4, i64_type_arg)  // [40, 48) main ret_val
		);

		// Pass the command line arguments only if main signature specifies it.
		if (main_has_args) {
			start_function.bc.insert(
				start_function.bc.end(),
				{
					MAKE_BYTECODE_INSTRUCTION(init_bany_type, 5, i64_type_arg),  // [48, 56) argc
					MAKE_BYTECODE_INSTRUCTION(
						init_bany_type, 6, argv_ptr_type_arg
					),                                                        // [56, 72) *argv
					MAKE_BYTECODE_INSTRUCTION(mov_p64_imm, 48, args.size()),  // argc := args.size()
					MAKE_BYTECODE_INSTRUCTION(mov_pptr_pptr, 56, 8),  // argv := argv_internal
				}
			);
		}

		start_function.bc.insert(
			start_function.bc.end(),
			{
				MAKE_BYTECODE_INSTRUCTION(stepGil, 0, 0),  // We need to acquire GIL
				MAKE_BYTECODE_INSTRUCTION(call_func, called_function_id, 0),  // call main
				MAKE_BYTECODE_INSTRUCTION(mov_p64_p64, 0, 40),  // ret_val := main_ret_val
				MAKE_BYTECODE_INSTRUCTION(mov_p64_imm, 32, 0),  // ix := 0
				MAKE_BYTECODE_INSTRUCTION(
					init_bany_type, 5, str_ptr_type_arg
				),  // [48, 64) ptr_tmp_store
			}
		);

		// After 'main' returned, free all the allocated strings in the argv table.
		for ([[maybe_unused]] const auto& arg: args) {
			start_function.bc.insert(
				start_function.bc.end(),
				{
					MAKE_BYTECODE_INSTRUCTION(
						anyArrayLoad_bany_pptr, 5, 8
					),  // ptr_tmp_store := argv_internal[ix]
					MAKE_BYTECODE_INSTRUCTION(ext_p64_type, 32, str_ptr_type_arg),
					MAKE_BYTECODE_INSTRUCTION(free_pptr, 48, 0),    // free ptr_tmp_store
					MAKE_BYTECODE_INSTRUCTION(add_p64_imm, 32, 1),  // ++ix
				}
			);
		}

		// Lastly, free all the data allocated by the start function.
		start_function.bc.insert(
			start_function.bc.end(),
			{
				MAKE_BYTECODE_INSTRUCTION(free_pptr, 8, 0),  // free *argv_internal
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

	SafeVMThread::ScopedGilGuard::ScopedGilGuard(SafeVMThread& t): thread(t) {
		thread.acquireGil();
	}

	SafeVMThread::ScopedGilGuard::~ScopedGilGuard() {
		if (thread.has_gil) thread.releaseGil();
	}

#if defined(__clang__)
// @TODO: #2582 suppress code deduplication in Clang
#elif defined(__GNUG__)
	#pragma GCC push_options
	#pragma GCC optimize("-fno-crossjumping")
#endif
	// NOLINTBEGIN(cppcoreguidelines-avoid-goto)
	// NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
	void runInterpreter(
		const MicroInstruction* instr, std::byte*& local_stack, Frame*& frame, SafeVMThread& thread
	) {
#ifdef USE_TAIL_CALLS
		return instr->tc_opfun(instr, local_stack, frame, thread);

#elifdef USE_SWITCH_CASE
		while (true) {
			switch (static_cast<low::MicroOpcode>(instr->nontc_opcode)) {
	#define HANDLE_MICRO_INSTR(opcode_name)                                                         \
	case low::MicroOpcode::opcode_name: {                                                           \
		vm::OpFuns::op_##opcode_name(instr, local_stack, frame, thread);                            \
		if constexpr (::vm::ENABLE_VM_DETAIL_LOGGING)                                               \
			CORE_DEV_LOG(                                                                           \
				DVMDetails, "opcode, ", #opcode_name, ", ", thread.getThreadID().asInt(), ";\n"     \
			);                                                                                      \
		if constexpr (constexpr std::string_view opcode_str = #opcode_name; opcode_str == "exit") { \
			goto End;                                                                               \
		} else {                                                                                    \
			break;                                                                                  \
		}                                                                                           \
	}
	#include <vm/core/safe/low_program/micro_instruction_definitions.hpp>
	#undef HANDLE_MICRO_INSTR

			default: {
				CORE_PANIC("Unknown operator: ", u64(instr->nontc_opcode));
			}
			}
		}
	End:
#endif
	}

	// NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
	// NOLINTEND(cppcoreguidelines-avoid-goto)

#if defined(__clang__)
// @TODO: #2582 suppress code deduplication in Clang
#elif defined(__GNUG__)
	#pragma GCC pop_options
#endif

	std::vector<Ref<SafeVMValue>> SafeVMThread::executeFunction(
		const low::LowFuncData& start_function, const low::LowFuncData& func
	) {
		ScopedGilGuard gil_guard(*this);

		// Frame of the called function.
		Frame*     frame          = runtime_data.frame_stack_current;
		Frame*     orig_frame_ptr = frame;
		Frame      orig_frame_cpy = *runtime_data.frame_stack_current;
		std::byte* local_stack    = frame->local_stack;
		if (local_stack == nullptr) local_stack = runtime_data.local_stack_base;
		auto orig_block_stack_size
			= usize(frame->local_block_ref_stack_end - frame->local_block_ref_stack_base);

		frame->current_function           = &start_function;
		frame->local_block_ref_stack_base = runtime_data.block_ref_stack_base;
		frame->local_block_ref_stack_end  = runtime_data.block_ref_stack_base;

		const auto* instr = start_function.bc.data();

		runInterpreter(instr, local_stack, frame, *this);

		CORE_ASSERT(
			frame == orig_frame_ptr,
			"After executing function we have to return to original place in call stack"
		);
		// @note: The return value is the only block left on the block stack.
		CORE_ASSERT(
			frame->local_block_ref_stack_end - frame->local_block_ref_stack_base
				>= orig_block_stack_size + func.result_types.size(),
			"After function execution, there should be enough blocks on the stack to retrieve "
			"result."
		);

		exit_value_storage = { std::vector<Ref<SafeVMValue>>{} };
		for (u64 idx = 0; idx < func.result_types.size(); idx++) {
			exit_value_storage.value().emplace_back(safe_process.createVMValue(
				func.result_types[idx],
				Pointer(frame->local_block_ref_stack_base[orig_block_stack_size + idx], 0)
			));
		}

		for (auto block_ptr = frame->local_block_ref_stack_base + orig_block_stack_size;
		     block_ptr < frame->local_block_ref_stack_end;
		     block_ptr++) {
			auto& block = *block_ptr;
			process_memory.freeBlockData(block);
			process_memory.decreaseBlockRefcount(block);
		}
		*orig_frame_ptr = orig_frame_cpy;

		return exit_value_storage.value();
	}

	// executeFunction end

	/**
	 * @brief Starts the execution of a function with a given name and arguments.
	 */
	void SafeVMThread::run(const std::string& func_name, const RunArguments& run_arguments) {
		for (const auto& [global, id, name]: process_program->getGlobals().allData()) {
			auto block_ref
				= Ref(runtime_data.global_block_ref_buffer_base[global->global_block_idx]);
			if (not process_memory.isGlobalInitialized(block_ref)) {
				variant_match(global->init) {
					variant_case(low::GlobalCtorDtor, ctor_dtor) {
						if (ctor_dtor.ctor_name.has_value()) {
							const auto& func = *process_program->getFunctions()
							                        .atMaybe(ctor_dtor.ctor_name.value())
							                        .value();
							low::LowFuncData start_function = createStartFunctionFor(func, {});
							executeFunction(start_function, func);
						}
					}
					variant_case(low::GlobalInitialValue, value_init) {
						// Copy the constant initial value bytes directly to the global's memory.
						process_memory.initializeBlockFromConstValue(block_ref, value_init.value);
					}
				}
				process_memory.setGlobalInitialized(block_ref);
			}
		}

		const auto& maybe_func
			= process_program->getFunctions().atMaybe(base::StrID(func_name.data()));
		if (!maybe_func.has_value()) {
			throw exceptions::VMRuntimeException(
				base::strConcat("Called function '", func_name, "' does not exist.")
			);
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
		applyEvent(te::Finish{ std::vector<Ref<IVMValue>>(exit_value.begin(), exit_value.end()) });
	}

	void SafeVMThread::execGlobalDestructors() {
		const auto& executing_program = process_program;
		auto        globals           = executing_program->getGlobals().allData();
		// Destructors should run in reverse order of construction so that any object depending on
		// earlier-created resources is destroyed first, preventing use-after-destruction and
		// keeping teardown safe and logically consistent.
		for (const auto& [global, id, name]: std::ranges::reverse_view(globals)) {
			auto* ctor_dtor = std::get_if<low::GlobalCtorDtor>(&global->init);
			if (ctor_dtor && ctor_dtor->dtor_name.has_value()) {
				const auto& func = *executing_program->getFunctions()
				                        .atMaybe(ctor_dtor->dtor_name.value())
				                        .expect(
											"Called function does not exist: "
											+ ctor_dtor->dtor_name.value().str()
										);
				low::LowFuncData start_function = createStartFunctionFor(func, {});
				executeFunction(start_function, func);
			}
		}
	}

	std::expected<low::LowCodePosition, api::ApiError> SafeVMThread::getCurrentPosition(
		base::Optional<usize> frame_idx
	) {
		variant_match(getThreadState()) {
			variant_case_novalue(ts::Paused) {
				auto frame = runtime_data.frame_stack_current;
				// In caller frames, the instruction pointer rests on the return address (the
				// instruction after the call). We must adjust it backward by 1 to point to the
				// actual call site.
				bool call_adjustment = false;
				if_opt_some(frame_idx, frame_index) {
					u64 frames = getNumberOfCurrentStackFrames();
					if (frame_index >= frames)
						return std::unexpected(api::ApiError{
							api::OtherError{ "Frame index out of bounds" } });

					frame = &getStackFrame(frame_index);
					if (frame_index + 1 != frames) call_adjustment = true;
				}

				auto& func = *frame->current_function;

				return low::LowCodePosition{
					.function = &func,
					.instruction_index
					= static_cast<u64>(frame->instr - func.bc.data() - (call_adjustment ? 1 : 0)),
				};
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

	bool SafeVMThread::isCallableFunctionID(usize id) {
		return id != SafeVMThread::START_FUNCTION_ID;
	}

	u64 SafeVMThread::getNumberOfCurrentStackFrames() const {
		// +1 because frame_stack_current points to the current frame, not the next free slot.
		return u64(runtime_data.frame_stack_current - runtime_data.frame_stack_base) + 1;
	}

	Frame& SafeVMThread::getStackFrame(u64 frame_index) {
		return runtime_data.frame_stack_base[frame_index];
	}

	void SafeVMThread::updateGlobalDataBufferPointers(
		GlobalBufferPointersGeneric global_buffer_pointers
	) {
		runtime_data.global_data_buffer_base      = global_buffer_pointers.data_buffer_base;
		runtime_data.global_block_ref_buffer_base = global_buffer_pointers.blocks_buffer_base;
	}
}
