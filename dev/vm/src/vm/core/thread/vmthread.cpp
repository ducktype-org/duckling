
#include "vmthread.hpp"

#include "low_program/instruction.hpp"
#include "low_program/opcodes.hpp"
#include "op_case.hpp"
#include "opcodes_functions.hpp"
#include "opcodes_functions_debug.hpp"

#include <base/exceptions.hpp>
#include <base/int_conv.hpp>
#include <base/ints.hpp>
#include <base/optional.hpp>
#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <vm/api/data/response.hpp>
#include <vm/api/data/status.hpp>
#include <vm/core/kill_process_exception.hpp>
#include <vm/core/process/memory/pointer.hpp>
#include <vm/core/process/type_metadata/definitions.hpp>
#include <vm/core/process/type_metadata/type.hpp>
#include <vm/core/supervisor/supervisor.hpp>

#include <cstring>
#include <iostream>
#include <mutex>
#include <utility>
#include <variant>

namespace vm {
#ifdef USE_TAIL_CALLS
	#define DEFINE_BYTECODE_INSTRUCTION(OPCODE_NAME, ARG_0, ARG_1) \
		Fix8Instruction { .opfun = OpFuns::op_##OPCODE_NAME, .arg0 = ARG_0, .arg1 = ARG_1 }
#else
	#define DEFINE_BYTECODE_INSTRUCTION(OPCODE_NAME, ARG_0, ARG_1)                                 \
		Fix8Instruction {                                                                          \
			.opcode = static_cast<u16>(low::OpcodeFix8::OPCODE_NAME), .arg0 = ARG_0, .arg1 = ARG_1 \
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
		auto opcode = OpFuns::getOpcodeFromOpFun(instr->opfun);
#else
		auto opcode = static_cast<u16>(instr->opcode);
#endif

		// Execute the instruction by calling the debug opcode function.
		OpFuns::DEBUG_OPFUNS.at(opcode)(instr, local_stack, frame, *this);

		runtime_data.frame_stack_current = frame;
		frame->local_stack               = local_stack;
		frame->instr                     = instr;
	}

	VMThread::ByteCode VMThread::createStartFunction(
		CRef<low::FuncData> func, const std::vector<std::string>& args
	) {
		// Just like in libc, the start function pushes the program arguments on to the stack and
		// performs the call to the actual function. After the called function returns, it
		// deinitializes the argv memory and exits, leaving one block on the block stack, which
		// contains the return value of the program.

		// Types
		auto called_func_type = executing_program->types->atMaybe(func->name)
		                            .expect("Expected the called function to exist!");
		auto called_return_type
			= called_func_type->getResultType().expect("Expected main to have a return value!");
		auto argv_type = executing_program->types->atMaybe(base::StrID("argv"))
		                     .expect("All programs are expected to have an existing argv type!");
		auto argv_ptr_type
			= executing_program->types->atMaybe(base::StrID("ptr_argv"))
		          .expect("All programs are expected to have an existing argv pointer type!");
		auto i64_type = executing_program->types->atMaybe(base::StrID("i64"))
		                    .expect("Type i64 is expected to exist!");

		// TypeIDs to pass to opcodes.
		i32  func_ret_type_id   = base::safeIntConv<i32>(called_return_type->getID().asInt());
		i32  argv_type_id       = base::safeIntConv<i32>(argv_type->getID().asInt());
		i32  argv_ptr_type_id   = base::safeIntConv<i32>(argv_ptr_type->getID().asInt());
		i32  i64_type_id        = base::safeIntConv<i32>(i64_type->getID().asInt());
		auto funcs              = executing_program->functions;
		i32  called_function_id = 0;
		for (u64 i = 0; i < funcs.size(); i++)
			if (func->name == funcs[i].name) called_function_id = base::safeIntConv<i32>(i);

		ByteCode bytecode;
		bytecode.reserve(5 + args.size() * 4 + 13);

		bytecode = {
			DEFINE_BYTECODE_INSTRUCTION(init_type, func_ret_type_id, 0),  // [0, 8) program ret_val
			DEFINE_BYTECODE_INSTRUCTION(init_type, argv_ptr_type_id, 0),  // [8, 24) *argv
			DEFINE_BYTECODE_INSTRUCTION(alloc_lptr_type, 8, argv_type_id),  // alloc argv
			DEFINE_BYTECODE_INSTRUCTION(init_type, i64_type_id, 0),         // [24, 32) ix
			DEFINE_BYTECODE_INSTRUCTION(init_type, i64_type_id, 0),         // [32, 40) temp_store
		};

		for (const auto& arg: args) {
			// @note: Since strings don't exist in the VM yet, the passed arguments, are converted
			// to ints.
			i32 converted_arg = base::safeIntConv<i32>(std::stoi(arg));
			bytecode.insert(
				bytecode.end(),
				{
					DEFINE_BYTECODE_INSTRUCTION(mov_l64_imm, 32, converted_arg),
					// @todo: This should be changed to 'store_lptr_l64_ofs' once it exists.
					DEFINE_BYTECODE_INSTRUCTION(store_lptr_l64_ofs, 8, 32),
					DEFINE_BYTECODE_INSTRUCTION(ext_l64, 24, 0),
					DEFINE_BYTECODE_INSTRUCTION(add_l64_imm, 24, 1),
				}
			);
		}

		bytecode.insert(
			bytecode.end(),
			{
				DEFINE_BYTECODE_INSTRUCTION(
					init_type, func_ret_type_id, 0
				),                                                       // [40, 48) call ret_val
				DEFINE_BYTECODE_INSTRUCTION(init_type, i64_type_id, 0),  // [48, 56] argc
				DEFINE_BYTECODE_INSTRUCTION(mov_l64_imm, 48, base::safeIntConv<i32>(args.size())),
				DEFINE_BYTECODE_INSTRUCTION(init_type, argv_ptr_type_id, 0),  // [56, 72) *argv
				DEFINE_BYTECODE_INSTRUCTION(mov_lptr_lptr, 56, 8),
				DEFINE_BYTECODE_INSTRUCTION(call_func, called_function_id, 0),
				// @todo: For now we assume that the return values are always i64. It's true for
		        // main, but won't be true once REPL arrives.
				DEFINE_BYTECODE_INSTRUCTION(mov_l64_l64, 0, 40),  // move the ret_val to 0th block
				DEFINE_BYTECODE_INSTRUCTION(free_lptr, 8, 0),     // free *argv
				DEFINE_BYTECODE_INSTRUCTION(deinit, 0, 0),        // temp
				DEFINE_BYTECODE_INSTRUCTION(deinit, 0, 0),        // called func ret_val
				DEFINE_BYTECODE_INSTRUCTION(deinit, 0, 0),        // ix
				DEFINE_BYTECODE_INSTRUCTION(deinit, 0, 0),        // argv_ptr
				// Here, only the return value remains on the stack.
				DEFINE_BYTECODE_INSTRUCTION(exit, 0, 0),
			}
		);
		return bytecode;
	}

#if defined(__clang__)
// @TODO: suppress code deduplication in Clang
#elif defined(__GNUG__)
	#pragma GCC push_options
	#pragma GCC optimize("-fno-crossjumping")
#endif
	// NOLINTBEGIN(cppcoreguidelines-avoid-goto)
	// NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
	i64 VMThread::internalCallFunction(
		CRef<low::FuncData> func, const std::vector<std::string>& args
	) {
		// Frame of the called function.
		Frame*     frame       = runtime_data.frame_stack_base;
		std::byte* local_stack = runtime_data.local_stack_base;
		if (local_stack + func->local_stack_size > runtime_data.local_stack_end)
			CORE_PANIC("VM stack overflow.");

		auto called_func_type = executing_program->types->atMaybe(func->name)
		                            .expect("Expected the called function to exist!");
		auto called_return_type
			= called_func_type->getResultType().expect("Expected main to have a return value!");
		frame->called_func_ret_size = called_return_type->getSize();

		ByteCode    start_function = createStartFunction(func, args);
		const auto* instr          = start_function.data();

#ifdef USE_TAIL_CALLS
		instr->opfun(instr, local_stack, frame, *this);

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

		goto* opcode_label[static_cast<u64>(instr->opcode)];

	#define HANDLE_OPCODE(opcode_name)                                          \
		LABEL_##opcode_name: {                                                  \
			vm::OpFuns::op_##opcode_name(instr, local_stack, frame, *this);     \
			if constexpr (constexpr std::string_view opcode_str = #opcode_name; \
			              opcode_str == "exit") {                               \
				goto End;                                                       \
			} else {                                                            \
				goto* opcode_label[static_cast<u64>(instr->opcode)];            \
			}                                                                   \
		}
	#include <vm/bytecode/opcode_definitions.hpp>

	#undef HANDLE_OPCODE

	End:

		POP_DIAGNOSTIC
#elif USE_SWITCH_CASE
		while (true) {
			switch (static_cast<low::OpcodeFix8>(instr->opcode)) {
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
				CORE_PANIC("Unknown operator:", u64(instr->opcode));
			}
			}
		}
	End:
#endif
		// The return value is the only block left on the block stack.
		i64  func_ret_val = derefStack<i64>(local_stack, 0);
		auto block        = frame->block_stack.back();
		frame->block_stack.pop_back();
		process_memory.freeBlock(block);

		return func_ret_val;
	}

	// internalCallFunction end

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
			CORE_PANIC("unexpected execution status");
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
				CORE_PANIC("resumed with paused status");
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
	 * @brief Starts the execution of the program.
	 */
	void VMThread::run(CRef<low::LowVMProgram> program, const std::vector<std::string>& args) {
		respondExecutionRequest(api::Running{});
		executing_program = program;
		try {
			i64 exit_code
				= internalCallFunction(executing_program->functions.at(base::StrID("main")), args);
			respondExecutionRequest(api::ExecutionCompleted{ exit_code });
		} catch (KillProcessException) { respondExecutionRequest(api::ExecutionStopped{}); }
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

	std::expected<api::Response, api::CoreOperationError> VMThread::getCurrentPosition() {
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
				return std::unexpected(api::CoreOperationError{
					api::OtherError{ "wrong execution status while reading current position" } });
			}
		}
		CORE_UNREACHABLE();
	}

	void VMThread::setProcessStatus(const vm::api::ExecStatus& new_status) {
		status = new_status;
		process.onEvent(api::Executing{ new_status });
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

	bool VMThread::initThreadAndRun(
		CRef<vm::low::LowVMProgram> program, const std::vector<std::string>& args
	) {
		if (exec_thread)  // there is already a thread running
			return false;

		exec_thread = std::thread([this, program, args] {
			try {
				run(program, args);
				// @TODO: catch not general std::exception&
			} catch (const std::exception& e) {
				std::cerr << "VMThread has panicked: " << e.what() << "\n";
				respondExecutionRequest(api::ExecutionPanicked{});
			}
		});
		return waitForRunningResponse();
	}

	void VMThread::respondExecutionRequest(const api::ExecStatus& response) {
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
