
#include <mutex>
#include <utility>
#include <cstring>
#include <base/ints.hpp>
#include <base/exceptions.hpp>
#include <base/optional.hpp>
#include <code_data/instruction.hpp>
#include <code_data/opcodes.hpp>
#include <core/process/type_metadata/type.hpp>
#include <core/supervisor/supervisor.hpp>
#include <core/kill_process_exception.hpp>
#include <core/process/memory/pointer.hpp>
#include <api/data/response.hpp>
#include <api/data/status.hpp>
#include <base/variant.hpp>
#include "op_case.hpp"
#include "vmthread.hpp"
#include "opcodes_functions.hpp"
#include "opcodes_functions_debug.hpp"
#include <iostream>

namespace vm {
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


#if defined(__clang__)
// @TODO: suppress code deduplication in Clang
#elif defined(__GNUG__)
	#pragma GCC push_options
	#pragma GCC optimize("-fno-crossjumping")
#endif
	// NOLINTBEGIN(cppcoreguidelines-avoid-goto)
	// NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
	u64 VMThread::internalCallMain(CRef<FuncData> main_func) {
		// We create one artificial "pre" frame, that when main function returns
		// it will go to it and end execution.
		Frame* pre_frame = runtime_data.frame_stack_base;

#ifdef USE_TAIL_CALLS
		Fix8Instruction exit_instr{ .opfun = OpFuns::op_exit, .arg0 = 0, .arg1 = 0 };
#else
		Fix8Instruction exit_instr{ .opcode = static_cast<u16>(OpcodeFix8::exit),
			                        .arg0   = 0,
			                        .arg1   = 0 };
#endif

		pre_frame->instr       = &exit_instr;
		pre_frame->local_stack = runtime_data.local_stack_top;

		// Frame of the main function.
		Frame*     frame       = runtime_data.frame_stack_base + 1;
		std::byte* local_stack = runtime_data.local_stack_top;
		runtime_data.local_stack_top += main_func->stack_size;

		frame->next_args = runtime_data.local_stack_top;
		runtime_data.local_stack_top += main_func->next_arg_size;

		auto* instr = main_func->bc.data();

#ifdef USE_TAIL_CALLS
		instr->opfun(instr, local_stack, frame, *this);
		return runtime_data.frame_stack_base->regs.p64_reg_0;
#elif USE_COMPUTED_GOTO
		// We use computed-gotos here,
		// so we turn off pedantic warnings
		// for this case
		PUSH_DIAGNOSTIC
		_Pragma("GCC diagnostic ignored \"-Wpedantic\""
		) constexpr static std::array<void*, OP_CASES_COUNT>
			opcode_label = {


	#define HANDLE_OPCODE(opcode) (&&LABEL_##opcode),
	#include <code_data/opcodes_list.hpp>
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
	#include <code_data/opcodes_list.hpp>
	#undef HANDLE_OPCODE

	End:
		return runtime_data.frame_stack_base->regs.p64_reg_0;

		POP_DIAGNOSTIC
#elif USE_SWITCH_CASE
		while (true) {
			switch (static_cast<OpcodeFix8>(instr->opcode)) {
	#define HANDLE_OPCODE(opcode_name)                                      \
	case OpcodeFix8::opcode_name: {                                         \
		vm::OpFuns::op_##opcode_name(instr, local_stack, frame, *this);     \
		if constexpr (constexpr std::string_view opcode_str = #opcode_name; \
		              opcode_str == "exit") {                               \
			goto End;                                                       \
		} else {                                                            \
			break;                                                          \
		}                                                                   \
	}
	#include <code_data/opcodes_list.hpp>
	#undef HANDLE_OPCODE

			default: {
				CORE_PANIC("Unknown operator:", u64(instr->opcode));
			}
			}
		}
	End:
		return runtime_data.frame_stack_base->regs.p64_reg_0;

#endif
	}

	// internalCallMain end

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
			respondExecutionRequest(ExecutionResponse::Paused);
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
				respondExecutionRequest(ExecutionResponse::Running);
				return;
			}
			case ExecutionRequest::Stop: {
				throw KillProcessException{};
			}
			case ExecutionRequest::ExecuteOneStep: {
				respondExecutionRequest(ExecutionResponse::Running);

				executeOneStep();

				execution_request = ExecutionRequest::Pause;
				respondExecutionRequest(ExecutionResponse::Paused);
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
	void VMThread::run(CRef<VMProgram> program) {
		respondExecutionRequest(ExecutionResponse::Running);
		executing_code = program;
		try {
			internalCallMain(
				executing_code->getFuncByName(base::StrID("main")).expect("Expected main!")
			);
			respondExecutionRequest(ExecutionResponse::ExecutionCompleted);
		} catch (KillProcessException) {
			respondExecutionRequest(ExecutionResponse::ExecutionStopped);
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

		return waitForBrakepointResponse();
	}

	bool VMThread::step() {
		{
			std::unique_lock lock(execution_request_mutex);

			execution_request = ExecutionRequest::ExecuteOneStep;
			pause_cv.notify_all();
		}
		if (waitForRunningResponse()) {
			if (waitForBrakepointResponse()) return true;
		}
		return false;
	}

	cpp::result<api::Response, api::CoreOperationError> VMThread::getCurrentPosition() {
		variant_match(status) {
			variant_case_novalue(api::Paused) {
				auto frame = runtime_data.frame_stack_current;
				auto instr = frame->instr;

				for (size_t index = 0; index < executing_code->getNumberOfFunctions(); ++index) {
					const auto& func = executing_code->functions[index];
					if (func.bc.data() <= instr && instr < func.bc.data() + func.bc.size()) {
						return api::Response(api::response::CodePosition{
							.function_id  = static_cast<u64>(index),  // Assuming function_id is int
							.instr_number = static_cast<u64>(instr - func.bc.data()) });
					}
				}
			}
			variant_default {
				return cpp::failure(api::CoreOperationError{
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

	bool VMThread::initThreadAndRun(CRef<vm::VMProgram> program) {
		if (exec_thread)  // there is already a thread running
			return false;

		exec_thread = std::thread([this, program] {
			try {
				run(program);

				// @TODO: catch not general std::exception&
			} catch (const std::exception& e) {
				std::cerr << "VCPU PANICKED WITH: " << e.what() << "\n";
				respondExecutionRequest(ExecutionResponse::ExecutionPanicked);
			}
		});
		return waitForRunningResponse();
	}

	void VMThread::respondExecutionRequest(ExecutionResponse response) {
		switch (response) {
		case ExecutionResponse::Running:
			setProcessStatus(api::Running{});
			break;
		case ExecutionResponse::Paused:
			setProcessStatus(api::Paused{});
			break;
		case ExecutionResponse::ExecutionStopped:
			setProcessStatus(api::ExecutionStopped{});
			break;
		case ExecutionResponse::ExecutionCompleted:
			setProcessStatus(api::ExecutionCompleted{});
			break;
		case ExecutionResponse::ExecutionPanicked:
			setProcessStatus(api::ExecutionPanicked{});
			break;
		default:
			CORE_PANIC("unexpected execution response");
		}
		execution_response_queue.push(response);
	}

	bool VMThread::waitForStoppedResponse() {
		auto response = execution_response_queue.pop();
		return ExecutionResponse::ExecutionStopped == response
		    || ExecutionResponse::ExecutionCompleted == response
		    || ExecutionResponse::ExecutionPanicked == response;
	}

	bool VMThread::waitForBrakepointResponse() {
		return execution_response_queue.pop() == ExecutionResponse::Paused;
	}

	bool VMThread::waitForRunningResponse() {
		return execution_response_queue.pop() == ExecutionResponse::Running;
	}
}  // namespace vm
