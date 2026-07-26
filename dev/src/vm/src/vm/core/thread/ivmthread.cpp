#include "ivmthread.hpp"

#include <vm/core/process/ivmprocess.hpp>
#include <vm/core/thread/kill_process_exception.hpp>

namespace vm {
	std::expected<api::Response, api::ApiError> IVMThread::join() {
		if (!exec_thread || !exec_thread->joinable())
			return std::unexpected(api::ApiError{ api::JoinError{} });

		exec_thread->join();
		exec_thread.reset();
		setThreadStatus(api::NotStarted{});

		auto execution_status = execution_response_queue.pop();
		variant_match(execution_status) {
			variant_case(api::ExecutionCompleted, completed) {
				return api::Response(api::response::Empty());
			}
			variant_case(api::ExecutionStopped, stopped) {
				return api::Response(api::response::Empty());
			}
			variant_case(api::ExecutionPanicked, panicked) {
				return std::unexpected(api::ApiError(
					api::OtherError("Execution panicked with error: " + panicked.error_message)
				));
			}
			variant_default {
				return std::unexpected(api::ApiError(api::OtherError("Unexpected run status!")));
			}
		}
		CORE_UNREACHABLE();
	}

	bool IVMThread::isPauseRequested() {
		std::unique_lock lock(execution_request_mutex);
		return execution_request == ExecutionRequest::Pause;
	}

	bool IVMThread::isTerminateRequested() {
		{
			std::unique_lock lock(execution_request_mutex);
			if (execution_request == ExecutionRequest::Stop) return true;
		}
		return my_process.isExecutionPanicked();
	}

	bool IVMThread::waitForPausedResponse() {
		return std::holds_alternative<api::Paused>(execution_response_queue.pop());
	}

	bool IVMThread::waitForStoppedResponse() {
		auto response = execution_response_queue.pop();
		return std::holds_alternative<api::ExecutionStopped>(response)
		    || std::holds_alternative<api::ExecutionCompleted>(response)
		    || std::holds_alternative<api::ExecutionPanicked>(response);
	}

	bool IVMThread::waitForRunningResponse() {
		return std::holds_alternative<api::Running>(execution_response_queue.pop());
	}

	bool IVMThread::waitForExprEvaluation() {
		return std::holds_alternative<api::ExprExecutionCompleted>(execution_response_queue.pop());
	}

	void IVMThread::respondExecutionRequest(const api::ProcStatus& response) {
		setProcessStatus(response);
		execution_response_queue.push(response);
	}

	void IVMThread::setProcessStatus(const api::ProcStatus& new_status) {
		setThreadStatus(new_status);
		my_process.setStatus(new_status, thread_id);
	}

	bool IVMThread::joinExecutionThread() {
		if (!exec_thread || !exec_thread->joinable()) return false;

		exec_thread->join();
		exec_thread.reset();
		setThreadStatus(api::NotStarted{});
		return true;
	}

	void IVMThread::safeRun(const std::string& func_name, const RunArguments& run_arguments) {
		try {
			run(func_name, run_arguments);
		} catch (const exceptions::VMRuntimeException& e) {
			std::cerr << "VMThread has panicked: " << e.what() << "\n";
			respondExecutionRequest(api::ExecutionPanicked{ e.what() });
		}
	}

	bool IVMThread::spawnThreadAndRun(
		const std::string& func_name, const RunArguments& run_arguments
	) {
		if (exec_thread)  // There is already a thread running.
			return false;

		exec_thread = std::thread(&IVMThread::safeRun, this, func_name, run_arguments);
		return waitForRunningResponse();
	}

	void IVMThread::runNoSpawn(const std::string& func_name, const RunArguments& run_arguments) {
		// @TODO: #2040 Make this function check if anyone else is executing anything,
		// or simplify the state checking, perhaps remove state from thread and move all the
		// state to the process?
		safeRun(func_name, run_arguments);
		waitForRunningResponse();
	}

	bool IVMThread::pause() {
		{
			std::unique_lock lock(execution_request_mutex);

			execution_request              = ExecutionRequest::Pause;
			execution_request_pending_flag = true;
		}

		return waitForPausedResponse();
	}

	bool IVMThread::resume() {
		{
			std::unique_lock lock(execution_request_mutex);

			execution_request = ExecutionRequest::Resume;
		}
		pause_cv.notify_all();

		return waitForRunningResponse();
	}

	bool IVMThread::step() {
		{
			std::unique_lock lock(execution_request_mutex);

			execution_request = ExecutionRequest::ExecuteOneStep;
			pause_cv.notify_all();
		}
		if (waitForRunningResponse()) {
			if (waitForPausedResponse()) return true;
		}
		return false;
	}

	bool IVMThread::stop() {
		{
			std::unique_lock lock(execution_request_mutex);

			execution_request              = ExecutionRequest::Stop;
			execution_request_pending_flag = true;
		}
		pause_cv.notify_all();

		return waitForStoppedResponse();
	}

	bool IVMThread::hasActiveThread() const { return exec_thread && exec_thread->joinable(); }

	/**
	 * @details Assumes that the instruction in the frame is to be executed before AND after running
	 * this function.
	 */
	void IVMThread::breakActiveExecution() {
		std::unique_lock lock(execution_request_mutex);
		switch (execution_request) {
		case ExecutionRequest::Pause:
			respondExecutionRequest(api::Paused{});
			runDebuggerLoop(lock);
			execution_request_pending_flag = false;
			break;

		case ExecutionRequest::Stop:
			throw KillProcessException{};

		default:
			CORE_PANIC("Unexpected execution status");
		}
	}

	void IVMThread::notifyPaused() { pause_cv.notify_all(); }

	/**
	 * @details Assumes that the instruction in the frame is to be executed before AND after running
	 * this function.
	 */
	void IVMThread::runDebuggerLoop(std::unique_lock<std::mutex>& lock) {
		while (true) {
			// Loop invariant: the instruction in the frame is to be executed
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
				throw exceptions::VMResumedWithPausedStatusException();
			}
		}
	}

	void IVMThread::handleBreakpoint() {
		std::unique_lock lock(execution_request_mutex);
		setProcessStatus(api::Paused{});
		execution_request = ExecutionRequest::Pause;
		runDebuggerLoop(lock);
	}

}
