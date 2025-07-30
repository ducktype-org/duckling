
#include "vmthread.hpp"

#include "opcode_functions/opcodes_functions.hpp"
#include "opcode_functions/opcodes_functions_utils.hpp"

#include <base/exceptions.hpp>
#include <base/int_conv.hpp>
#include <base/ints.hpp>
#include <base/optional.hpp>
#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <vm/api/data/response.hpp>
#include <vm/api/data/status.hpp>
#include <vm/core/kill_process_exception.hpp>
#include <vm/core/process/exceptions.hpp>
#include <vm/core/process/memory/pointer.hpp>
#include <vm/core/process/type_metadata/definitions.hpp>
#include <vm/core/process/type_metadata/type.hpp>
#include <vm/core/process/vmprocess.hpp>
#include <vm/core/supervisor/supervisor.hpp>
#include <vm/core/thread/low_program/opcodes.hpp>

#include <cstring>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace vm {
#ifdef USE_TAIL_CALLS
	#define MAKE_BYTECODE_INSTRUCTION(OPCODE_NAME, ARG_0, ARG_1) \
		MicroInstruction { .tc_opfun = OpFuns::op_##OPCODE_NAME, .arg0 = ARG_0, .arg1 = ARG_1 }
#else
	#define MAKE_BYTECODE_INSTRUCTION(OPCODE_NAME, ARG_0, ARG_1)                           \
		MicroInstruction {                                                                 \
			.nontc_opcode = static_cast<u16>(low::OpcodeFix8::OPCODE_NAME), .arg0 = ARG_0, \
			.arg1 = ARG_1                                                                  \
		}
#endif

	VMThread::VMThread(VMProcess& process):
		  runtime_data(process.getMemory().initializeFrameStack()),
		  process(process),
		  process_memory(process.getMemory()) {}

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
	void VMThread::executeOneStep() {
		Frame*     frame       = runtime_data.frame_stack_current;
		std::byte* local_stack = frame->local_stack;
		auto*      instr       = frame->instr;

#ifdef USE_TAIL_CALLS
		auto opcode = OpFuns::getOpcodeFromOpFun(instr->tc_opfun);
#else
		auto opcode = static_cast<u16>(instr->nontc_opcode);
#endif

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
	low::FuncData VMThread::createStartFunctionFor(
		const low::FuncData& func, const FunctionRunArguments& func_args
	) {
		low::FuncData start_function;
		start_function.name     = base::StrID("vm_start_function");
		start_function.arg_size = 0;
		start_function.ret_size = 0;

		// @note All the following are guaranteed to exist or their existence was checked earlier.
		auto called_func_type = executing_program->types->at(func.name);
		auto i64_type         = executing_program->types->at(base::StrID("i64"));

		u64  i64_type_id        = i64_type->getID().asInt();
		auto funcs              = executing_program->functions;
		u64  called_function_id = 0;
		for (u64 i = 0; i < funcs.size(); i++)
			if (func.name == funcs[i].name) called_function_id = i;

		u64 stack_top = 0;
		// @todo: VM functions should be able to return and take as parameters any VM type.
		// For now we assume we can only pass and return arguments of i64 type.
		// This should be changed in the issue #721.

		// Initialize an exit code/return value spot. In case of non void functions the exit_code is
		// the return value of the function. Void functions always return with the exit_code = 0.
		start_function.bc.push_back(MAKE_BYTECODE_INSTRUCTION(init_lany_type, 0, i64_type_id));

		stack_top += i64_type->getSize();

		for (u64 i = 0; i < func_args.size(); i++) {
			i64  converted_arg = func_args[i];
			auto arg_type      = called_func_type->getNthParameterType(i).expect(
                "Wrong number of passed arguments!"
            );
			u64 arg_type_id = arg_type->getID().asInt();
			start_function.bc.push_back(MAKE_BYTECODE_INSTRUCTION(init_lany_type, 0, arg_type_id));
			start_function.bc.push_back(MAKE_BYTECODE_INSTRUCTION(
				mov_l64_imm, stack_top, Memory::interpret<u64>(converted_arg)
			));
			stack_top += arg_type->getSize();
		}

		start_function.bc.insert(
			start_function.bc.end(),
			{
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
	 * loading phase is not possible. This also results in the need to create the function in its
	 * low representation.
	 * @todo This is a mock implementation. Since strings and dynamic arrays don't exist in the VM
	 * yet, the passed arguments are expected to be strings representing a numerical value and are
	 * passed as `i64` to the main function. Additionally, in the future, we might want to add a
	 * separate start function builder because the start function creation will get a lot more
	 * complicated, after we start using VmValue or default value constructors which have to be
	 * invoked before main.
	 *
	 * This should change after issues #722 and #724.
	 */
	low::FuncData VMThread::createProgramStartFunction(
		const low::FuncData& func, const ProgramRunArguments& args
	) {
		low::FuncData start_function;
		start_function.name     = base::StrID("vm_start_function");
		start_function.arg_size = 0;
		start_function.ret_size = 0;

		// Types
		// @note All the following are guaranteed to exist or their existence was checked earlier.
		auto called_func_type   = executing_program->types->at(func.name);
		auto called_return_type = called_func_type->getResultType().value();
		auto argv_type          = executing_program->types->at(base::StrID("argv"));
		auto argv_ptr_type      = executing_program->types->at(base::StrID("ptr_argv"));
		auto i64_type           = executing_program->types->at(base::StrID("i64"));

		// TypeIDs to pass to opcodes.
		u64  func_ret_type_id   = called_return_type->getID().asInt();
		u64  argv_type_id       = argv_type->getID().asInt();
		u64  argv_ptr_type_id   = argv_ptr_type->getID().asInt();
		u64  i64_type_id        = i64_type->getID().asInt();
		auto funcs              = executing_program->functions;
		u64  called_function_id = 0;
		for (u64 i = 0; i < funcs.size(); i++)
			if (func.name == funcs[i].name) called_function_id = i;

		start_function.bc.insert(
			start_function.bc.end(),
			{
				MAKE_BYTECODE_INSTRUCTION(
					init_lany_type, 0, func_ret_type_id
				),  // [0, 8) program ret_val
				MAKE_BYTECODE_INSTRUCTION(init_lany_type, 8, argv_ptr_type_id),  // [8, 24) *argv
				MAKE_BYTECODE_INSTRUCTION(alloc_lptr_type, 8, argv_type_id),     // alloc argv
				MAKE_BYTECODE_INSTRUCTION(init_lany_type, 24, i64_type_id),      // [24, 32) ix
				MAKE_BYTECODE_INSTRUCTION(init_lany_type, 32, i64_type_id),  // [32, 40) temp_store
			}
		);

		for (const auto& arg: args) {
			// @todo: Since strings don't exist in the VM yet, the passed arguments, are converted
			// to ints. This should change after #722
			u64 converted_arg = static_cast<u64>(std::stoll(arg));
			start_function.bc.insert(
				start_function.bc.end(),
				{
					MAKE_BYTECODE_INSTRUCTION(mov_l64_imm, 32, converted_arg),
					MAKE_BYTECODE_INSTRUCTION(fixedSizeTableStore_lptr_lany, 8, 32),
					MAKE_BYTECODE_INSTRUCTION(ext_l64, 24, 0),
					MAKE_BYTECODE_INSTRUCTION(add_l64_imm, 24, 1),
				}
			);
		}

		start_function.bc.insert(
			start_function.bc.end(),
			{
				MAKE_BYTECODE_INSTRUCTION(
					init_lany_type, 40, func_ret_type_id
				),  // [40, 48) call ret_val
				MAKE_BYTECODE_INSTRUCTION(init_lany_type, 48, i64_type_id),       // [48, 56] argc
				MAKE_BYTECODE_INSTRUCTION(mov_l64_imm, 48, args.size()),
				MAKE_BYTECODE_INSTRUCTION(init_lany_type, 56, argv_ptr_type_id),  // [56, 72) *argv
				MAKE_BYTECODE_INSTRUCTION(mov_lptr_lptr, 56, 8),
				MAKE_BYTECODE_INSTRUCTION(call_func, called_function_id, 0),
				// @todo: For now we assume that the return values are always i64. It's true for
		        // main, but won't be true once REPL arrives.
				MAKE_BYTECODE_INSTRUCTION(mov_l64_l64, 0, 40),  // move the ret_val to 0th block
				MAKE_BYTECODE_INSTRUCTION(free_lptr, 8, 0),     // free *argv
				MAKE_BYTECODE_INSTRUCTION(deinit, 0, 0),        // temp
				MAKE_BYTECODE_INSTRUCTION(deinit, 0, 0),        // called func ret_val
				MAKE_BYTECODE_INSTRUCTION(deinit, 0, 0),        // ix
				MAKE_BYTECODE_INSTRUCTION(deinit, 0, 0),        // argv_ptr
				// Here, only the return value remains on the stack.
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
	i64 VMThread::executeFunction(const low::FuncData& start_function, const low::FuncData& func) {
		// Frame of the called function.
		Frame*     frame       = runtime_data.frame_stack_base;
		std::byte* local_stack = runtime_data.local_stack_base;

		auto called_func_return_type = *executing_program->types->at(func.name)->getResultType();
		frame->called_func_ret_size  = called_func_return_type->getSize();

		const auto* instr = start_function.bc.data();

#ifdef USE_TAIL_CALLS
		instr->tc_opfun(instr, local_stack, frame, *this);

#elif USE_COMPUTED_GOTO
		// We use computed-gotos here,
		// so we turn off pedantic warnings
		// for this case
		PUSH_DIAGNOSTIC
		_Pragma("GCC diagnostic ignored \"-Wpedantic\""
		) constexpr static std::array<void*, OP_CASES_COUNT>
			opcode_label = {


	#define HANDLE_OPCODE(opcode) (&&LABEL_##opcode),
	#include <vm/bytecode/opcode_definitions.hpp>

	#undef HANDLE_OPCODE
			};

		goto* opcode_label[static_cast<u64>(instr->nontc_opcode)];

	#define HANDLE_OPCODE(opcode_name)                                          \
		LABEL_##opcode_name: {                                                  \
			vm::OpFuns::op_##opcode_name(instr, local_stack, frame, *this);     \
			if constexpr (constexpr std::string_view opcode_str = #opcode_name; \
			              opcode_str == "exit") {                               \
				goto End;                                                       \
			} else {                                                            \
				goto* opcode_label[static_cast<u64>(instr->nontc_opcode)];      \
			}                                                                   \
		}
	#include <vm/bytecode/opcode_definitions.hpp>

	#undef HANDLE_OPCODE

	End:

		POP_DIAGNOSTIC
#elif USE_SWITCH_CASE
		while (true) {
			switch (static_cast<low::OpcodeFix8>(instr->nontc_opcode)) {
	#define HANDLE_OPCODE(opcode_name)                                                              \
	case low::OpcodeFix8::opcode_name: {                                                            \
		vm::OpFuns::op_##opcode_name(instr, local_stack, frame, *this);                             \
		if constexpr (constexpr std::string_view opcode_str = #opcode_name; opcode_str == "exit") { \
			goto End;                                                                               \
		} else {                                                                                    \
			break;                                                                                  \
		}                                                                                           \
	}
	#include <vm/bytecode/opcode_definitions.hpp>
	#undef HANDLE_OPCODE

			default: {
				CORE_PANIC("Unknown operator: ", u64(instr->nontc_opcode));
			}
			}
		}
	End:
#endif
		// @todo: VM functions should should be able to return any VM type, not just i64.
		// This should be changed in issue #721
		// @note: The return value is the only block left on the block stack.
		i64  func_ret_val = derefStack<i64>(local_stack, 0);
		auto block        = frame->block_stack.back();
		process_memory.freeBlock(block);
		frame->resetFrameData();

		return func_ret_val;
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
	 * @brief Handles execution request when `execution_request_break` bool is set.
	 * Used from the thread loop.
	 */
	void VMThread::breakActiveExecution() {
		std::unique_lock lock(execution_request_mutex);
		switch (execution_request) {
		case ExecutionRequest::Pause:
			respondExecutionRequest(api::Paused{});
			handlePausedExecution(lock);
			execution_request_break = false;
			break;

		case ExecutionRequest::Stop:
			throw KillProcessException{};

		default:
			CORE_PANIC("Unexpected execution status");
		}
	}

	/**
	 * @brief Main function of the VMThread "debug" mode, where the step by step execution can take
	 * place. After each step the execution status is set to `paused` and the VMThread waits for the
	 * next command. Mutex "execution_request_mutex" is held when the VM is executing the code.
	 *
	 * @param lock
	 */
	void VMThread::handlePausedExecution(std::unique_lock<std::mutex>& lock) {
		while (true) {
			pause_cv.wait(lock, [this] { return execution_request != ExecutionRequest::Pause; });

			switch (execution_request) {
			case ExecutionRequest::Resume: {
				execution_request = ExecutionRequest::NoRequest;
				respondExecutionRequest(api::Running{});
				return;
			}
			case ExecutionRequest::Stop: {
				throw KillProcessException{};
			}
			case ExecutionRequest::ExecuteOneStep: {
				respondExecutionRequest(api::Running{});

				executeOneStep();

				execution_request = ExecutionRequest::Pause;
				respondExecutionRequest(api::Paused{});
				break;
			}
			default:
				throw exceptions::VMResuemedWithPausedStatusException();
			}
		}
	}

	/**
	 * @brief Function to be called when the VMThread hits a breakpoint.
	 */
	void VMThread::handleBreakpoint() {
		std::unique_lock lock(execution_request_mutex);
		setProcessStatus(api::Paused{});
		execution_request = ExecutionRequest::Pause;
		this->handlePausedExecution(lock);
	}

	/**
	 * @brief Starts the execution of a function with a given name and arguments.
	 */
	void VMThread::run(
		CRef<low::LowVMProgram> program,
		const std::string&      func_name,
		const RunArguments&     run_arguments
	) {
		respondExecutionRequest(api::Running{});

		executing_program = program;
		for (const auto& [global, id, name]: program->global_data.allData()) {
			// Insert the global data if it hasn't been initialized; then run constructor if present
			if (process_memory.tryInsertGlobalData(id, global->type)
			    && global->ctor_name.has_value()) {
				try {
					const auto& func = *executing_program->functions
					                        .atMaybe(base::StrID(global->ctor_name.value()))
					                        .expect(
												"Called function does not exist: "
												+ global->ctor_name.value().str()
											);
					low::FuncData start_function;
					start_function = createStartFunctionFor(func, {});
					i64 exit_code  = executeFunction(start_function, func);
					respondExecutionRequest(api::ExecutionCompleted{ exit_code });
				} catch (const KillProcessException& e) {
					respondExecutionRequest(api::ExecutionPanicked{ e.what() });
				}
			}
		}

		try {
			const auto& func = *executing_program->functions.atMaybe(base::StrID(func_name.data()))
			                        .expect("Called function does not exist: " + func_name);
			low::FuncData start_function;
			variant_match(run_arguments) {
				variant_case(ProgramRunArguments, program_run_arguments) {
					start_function = createProgramStartFunction(func, program_run_arguments);
				}
				variant_case(FunctionRunArguments, function_run_data) {
					start_function = createStartFunctionFor(func, function_run_data);
				}
			}

			i64 exit_code = executeFunction(start_function, func);
			respondExecutionRequest(api::ExecutionCompleted{ exit_code });
		} catch (const KillProcessException& e) {
			respondExecutionRequest(api::ExecutionPanicked{ e.what() });
		}
	}

	/**
	 * @brief Function to be called when the VMProcess is destroyed.
	 */
	void VMThread::execGlobalDestructors(CRef<low::LowVMProgram> program) {
		executing_program = program;
		for (const auto& [global, id, name]: executing_program->global_data.allData()) {
			if (global->dtor_name.has_value()) {
				try {
					const auto& func = *executing_program->functions
					                        .atMaybe(base::StrID(global->dtor_name.value()))
					                        .expect(
												"Called function does not exist: "
												+ global->dtor_name.value().str()
											);
					low::FuncData start_function;
					start_function = createStartFunctionFor(func, {});
					i64 exit_code  = executeFunction(start_function, func);
					respondExecutionRequest(api::ExecutionCompleted{ exit_code });
				} catch (const KillProcessException& e) {
					respondExecutionRequest(api::ExecutionPanicked{ e.what() });
				}
			}
		}
	}

	bool VMThread::stop() {
		{
			std::unique_lock lock(execution_request_mutex);

			execution_request       = ExecutionRequest::Stop;
			execution_request_break = true;
		}
		pause_cv.notify_all();

		return waitForStoppedResponse();
	}

	bool VMThread::resume() {
		{
			std::unique_lock lock(execution_request_mutex);

			execution_request = ExecutionRequest::Resume;
		}
		pause_cv.notify_all();

		return waitForRunningResponse();
	}

	bool VMThread::pause() {
		{
			std::unique_lock lock(execution_request_mutex);

			execution_request       = ExecutionRequest::Pause;
			execution_request_break = true;
		}

		return waitForBreakpointResponse();
	}

	bool VMThread::step() {
		{
			std::unique_lock lock(execution_request_mutex);

			execution_request = ExecutionRequest::ExecuteOneStep;
			pause_cv.notify_all();
		}
		if (waitForRunningResponse()) {
			if (waitForBreakpointResponse()) return true;
		}
		return false;
	}

	std::expected<api::Response, api::ApiError> VMThread::getCurrentPosition() {
		variant_match(status) {
			variant_case_novalue(api::Paused) {
				auto frame = runtime_data.frame_stack_current;
				auto instr = frame->instr;

				for (size_t index = 0; index < executing_program->functions.size(); ++index) {
					const auto& func = executing_program->functions[index];
					if (func.bc.data() <= instr && instr < func.bc.data() + func.bc.size()) {
						return api::Response(api::response::CodePosition{
							.function_id  = static_cast<u64>(index),  // Assuming function_id is int
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

	void VMThread::setProcessStatus(const vm::api::ProcStatus& new_status) {
		status = new_status;
		process.onEvent(new_status);
	}

	bool VMThread::isPauseRequested() {
		std::unique_lock lock(execution_request_mutex);
		return execution_request == ExecutionRequest::Pause;
	}

	bool VMThread::isTerminateRequested() {
		std::unique_lock lock(execution_request_mutex);
		return execution_request == ExecutionRequest::Stop;
	}

	void VMThread::notifyPaused() { pause_cv.notify_all(); }

	bool VMThread::spawnThreadAndRun(
		CRef<low::LowVMProgram> program,
		const std::string&      func_name,
		const RunArguments&     run_arguments
	) {
		if (exec_thread)  // There is already a thread running.
			return false;

		exec_thread = std::thread([this, program, func_name, run_arguments] {
			try {
				run(program, func_name, run_arguments);
			} catch (const exceptions::VMRuntimeException& e) {
				std::cerr << "VMThread has panicked: " << e.what() << "\n";
				respondExecutionRequest(api::ExecutionPanicked{ e.what() });
			}
		});
		return waitForRunningResponse();
	}

	void VMThread::respondExecutionRequest(const api::ProcStatus& response) {
		setProcessStatus(response);
		execution_response_queue.push(response);
	}

	bool VMThread::waitForStoppedResponse() {
		auto response = execution_response_queue.pop();
		return std::holds_alternative<api::ExecutionStopped>(response)
		    || std::holds_alternative<api::ExecutionCompleted>(response)
		    || std::holds_alternative<api::ExecutionPanicked>(response);
	}

	bool VMThread::waitForBreakpointResponse() {
		return std::holds_alternative<api::Paused>(execution_response_queue.pop());
	}

	bool VMThread::waitForRunningResponse() {
		return std::holds_alternative<api::Running>(execution_response_queue.pop());
	}
}  // namespace vm
