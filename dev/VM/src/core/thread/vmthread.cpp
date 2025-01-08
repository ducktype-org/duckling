
#include <mutex>
#include <utility>
#include <cstring>
#include <base/ints.hpp>
#include <base/exceptions.hpp>
#include <base/optional.hpp>
#include <code_data/instruction.hpp>
#include <code_data/opcodes.hpp>
#include <code_data/code.hpp>
#include <core/process/type_metadata/type.hpp>
#include <core/supervisor/supervisor.hpp>
#include <core/kill_process_exception.hpp>
#include <core/process/memory/pointer.hpp>
#include "api/data/response.hpp"
#include "api/data/status.hpp"
#include "base/variant.hpp"
#include "op_case.hpp"
#include "vmthread.hpp"
#include "opcodes.hpp"
#include "opcodes_debug.hpp"

namespace vm {
	VMThread::VMThread(VMProcess& process):
		  runtime_data(process.getMemory().initializeFrameStack()),
		  process(process),
		  process_memory(process.getMemory()),
		  process_types(process.getTypeMetadata()) {}

	RETURN_TYPE OpFuns::handle_execution_break(OPFUN_ARGS) {
		{
			save_execution_state(instr, local_stack, frame, thread);

			thread.handleExecutionBreak();

			// Restore current registers and flow, because
			// they could be changed when doing "step by step" execution.
			frame       = thread.runtime_data.frame_stack_current;
			instr       = frame->instr;
			local_stack = frame->local_stack;
		}
		OPFUN_CONT(1);
	}

	RETURN_TYPE OpFuns::save_execution_state(OPFUN_ARGS) {
		{
			// Save current registers and flow.
			frame->instr                            = instr;
			frame->local_stack                      = local_stack;
			thread.runtime_data.frame_stack_current = frame;
		}
	}

	void VMThread::executeOneStep() {
		Frame*     frame       = runtime_data.frame_stack_current;
		std::byte* local_stack = frame->local_stack;
		auto*      instr       = frame->instr;

#ifdef USE_TAIL_CALLS
		auto opcode = OpFuns::getOpcodeFromOpFun(instr->opfun);
#else
		auto opcode = static_cast<u16>(instr->opcode);
#endif

		// Execute the instruction
		OpFuns::debug_opfuns.at(opcode)(instr, local_stack, frame, *this);

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

	u64 VMThread::internalCallMain(const FuncData& main_func) {
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
		runtime_data.local_stack_top += main_func.stack_size;

		frame->next_args = runtime_data.local_stack_top;
		runtime_data.local_stack_top += main_func.next_arg_size;

		auto* instr = main_func.bc.data();

#ifdef USE_TAIL_CALLS
		instr->opfun(instr, local_stack, frame, *this);
		return runtime_data.frame_stack_base->regs.p64_reg_0;
#endif

#ifdef USE_COMPUTED_GOTO
		constexpr static std::array<void*, OP_CASES_COUNT> opcode_label = {
	#define DEF_OPCODE(opcode) (&&LABEL_##opcode),
	#include <code_data/opcodes_list.hpp>
	#undef DEF_OPCODE
		};

		goto* opcode_label[static_cast<u64>(instr->opcode)];

	#define DEF_OPCODE(opcode_name)                                         \
		LABEL_##opcode_name: {                                              \
			vm::OpFuns::op_##opcode_name(instr, local_stack, frame, *this); \
			goto* opcode_label[static_cast<u64>(instr->opcode)];            \
		}
	#define DEF_OPCODE_END(opcode_name)                                     \
		LABEL_##opcode_name: {                                              \
			vm::OpFuns::op_##opcode_name(instr, local_stack, frame, *this); \
			goto End;                                                       \
		}
	#include <code_data/opcodes_list.hpp>
	#undef DEF_OPCODE
	#undef DEF_OPCODE_END

	End:
		return runtime_data.frame_stack_base->regs.p64_reg_0;
#endif

#ifdef USE_SWITCH_CASE
		while (true) {
			switch (static_cast<OpcodeFix8>(instr->opcode)) {
	#define DEF_OPCODE(opcode_name)                                     \
	case OpcodeFix8::opcode_name: {                                     \
		vm::OpFuns::op_##opcode_name(instr, local_stack, frame, *this); \
		break;                                                          \
	}
	#define DEF_OPCODE_END(opcode_name)                                 \
	case OpcodeFix8::opcode_name: {                                     \
		vm::OpFuns::op_##opcode_name(instr, local_stack, frame, *this); \
		goto End;                                                       \
	}
	#include <code_data/opcodes_list.hpp>
	#undef DEF_OPCODE
	#undef DEF_OPCODE_END

			default: {
				CORE_PANIC("Unknown operator:", u64(instr->opcode));
			}
			}
		}
	End:
		return runtime_data.frame_stack_base->regs.p64_reg_0;

#endif
	}  // internalCallMain end

#if defined(__clang__)
// @TODO: suppress code deduplication in Clang
#elif defined(__GNUG__)
	#pragma GCC pop_options
#endif

	/**
	 * @brief Handle execution status when "not running" flag is set.
	 * It's only legal to change the execution status to `paused` or `terminated`.
	 */
	void VMThread::handleExecutionBreak() {
		std::unique_lock lock(execution_request_mutex);
		switch (execution_request) {
		case ExecutionRequest::Pause:
			handleExecutionPauseRequest(lock);
			execution_request_break = false;
			break;

		case ExecutionRequest::Terminate:
			throw KillProcessException{};

		default:
			CORE_PANIC("unexpected execution status");
		}
	}

	/**
	 * @brief Main function of the VMThread "debug" mode, where the step by step execution takes
	 * place. After each step the execution status is set to `paused` and the VMThread waits for the
	 * next command. Mutex is held when the VM is executing the code.
	 *
	 * @param lock
	 */
	void VMThread::handleExecutionPauseRequest(std::unique_lock<std::mutex>& lock) {
		while (true) {
			setProcessStatus(vm::api::Paused{}, true);
			pause_cv.wait(lock, [this] { return execution_request != ExecutionRequest::Pause; });

			switch (execution_request) {
			case ExecutionRequest::Resume: {
				setProcessStatus(vm::api::Running{}, true);
				execution_request = ExecutionRequest::NoRequest;
				return;
			}
			case ExecutionRequest::Terminate: {
				throw KillProcessException{};
			}
			case ExecutionRequest::ExecuteOneStep: {
				setProcessStatus(vm::api::Running{}, true);
				executeOneStep();
				execution_request = ExecutionRequest::Pause;
				break;
			}
			default:
				CORE_PANIC("resumed with paused status");
			}
		}
	}

	void VMThread::handleBreakpoint() {
		std::unique_lock lock(execution_request_mutex);
		execution_request = ExecutionRequest::Pause;
		this->handleExecutionPauseRequest(lock);
	}

	void VMThread::run(Ref<const Code> code) {
		// @TODO: ensure correct status

		setProcessStatus(api::Running{}, true);
		executing_code = code;
		try {
			internalCallMain(executing_code->functions[code->main_id]);
			setProcessStatus(api::ExecutionCompleted{}, true);
		} catch (KillProcessException) { setProcessStatus(api::Panicked{}, true); }
	}

	void VMThread::stop() {
		std::unique_lock lock(execution_request_mutex);

		execution_request       = ExecutionRequest::Terminate;
		execution_request_break = true;
		pause_cv.notify_all();
	}

	bool VMThread::resume() {
		std::unique_lock lock(execution_request_mutex);

		execution_request = ExecutionRequest::Resume;
		pause_cv.notify_all();
		return true;
	}

	bool VMThread::pause() {
		std::unique_lock lock(execution_request_mutex);

		execution_request       = ExecutionRequest::Pause;
		execution_request_break = true;
		return true;
	}

	bool VMThread::step() {
		std::unique_lock lock(execution_request_mutex);

		execution_request = ExecutionRequest::ExecuteOneStep;
		pause_cv.notify_all();
		return true;
	}

	cpp::result<api::Response, api::CoreOperationError> VMThread::getCurrentPosition() {
		variant_match(status) {
			variant_case_novalue(api::Paused) {
				auto frame = runtime_data.frame_stack_current;
				auto instr = frame->instr;

				for (size_t index = 0; index < executing_code->functions.size(); ++index) {
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

	void VMThread::setProcessStatus(const vm::api::ExecStatus& new_status, bool is_blocking) {
		status = new_status;
		process.setExecutionStatus(new_status, is_blocking);
	}

	bool VMThread::isPauseRequested() {
		std::unique_lock lock(execution_request_mutex);
		return execution_request == ExecutionRequest::Pause;
	}

	bool VMThread::isTerminateRequested() {
		std::unique_lock lock(execution_request_mutex);
		return execution_request == ExecutionRequest::Terminate;
	}

	void VMThread::notifyPaused() { pause_cv.notify_all(); }

	bool VMThread::initThread(Ref<const vm::Code> code) {
		if (exec_thread)  // there is already a thread running
			return false;

		exec_thread = std::thread([this, code] {
			try {
				run(code);

				// @TODO: catch not general std::exception&
			} catch (const std::exception& e) {
				std::cerr << "VCPU PANICKED WITH: " << e.what() << "\n";
				setProcessStatus(api::Panicked{ e }, true);
			}
		});
		return true;
	}
}  // namespace vm
