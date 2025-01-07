
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
#include "api/data/status.hpp"
#include "op_case.hpp"
#include "vmthread.hpp"
#include "opcodes.hpp"

namespace vm {
	VMThread::VMThread(VMProcess& process):
		  runtime_data(process.getMemory().initializeFrameStack()),
		  process(process),
		  process_memory(process.getMemory()),
		  process_types(process.getTypeMetadata()) {
		// @TODO: not loaded status
		notifyProcess(api::NotStarted{});
	}

#if defined(__clang__)
// @TODO: suppress code deduplication in Clang
#elif defined(__GNUG__)
	#pragma GCC push_options
	#pragma GCC optimize("-fno-crossjumping")
#endif


	void VMThread::executeOneStep() {
		Frame*     frame       = runtime_data.frame_stack_current;
		std::byte* local_stack = frame->local_stack;
		auto*      instr       = frame->instr;
		// Here we have a problem...
	}

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

	void VMThread::run(Ref<const Code> code) {
		// @TODO: ensure correct status

		notifyProcess(api::Running{});

		executing_code = code;
		try {
			internalCallMain(executing_code->functions[code->main_id]);
			notifyProcess(api::Terminated{});
		} catch (KillProcessException) { notifyProcess(api::Terminated{}); }
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
			notifyProcess(vm::api::Paused{});
			pause_cv.wait(lock, [this] { return execution_request != ExecutionRequest::Pause; });

			switch (execution_request) {
			case ExecutionRequest::Resume: {
				notifyProcess(vm::api::Running{});
				execution_request = ExecutionRequest::NoRequest;
				return;
			}
			case ExecutionRequest::Terminate: {
				throw KillProcessException{};
			}
			case ExecutionRequest::ExecuteOneStep: {
				notifyProcess(vm::api::Running{});
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

	void VMThread::notifyProcess(vm::api::ExecStatus new_status) {
		process.onEvent(api::Executing{ std::move(new_status) });
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
				process.onEvent(api::ProcStatus{ api::Panicked{ e } });
			}
		});
		return true;
	}
}  // namespace vm
