#include "ivmthread.hpp"

#include <vm/core/process/vmprocess.hpp>
#include <vm/core/thread/kill_process_exception.hpp>

std::expected<vm::api::Response, vm::api::ApiError> vm::IVMThread::join() {
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

bool vm::IVMThread::isPauseRequested() {
	std::unique_lock lock(execution_request_mutex);
	return execution_request == ExecutionRequest::Pause;
}

bool vm::IVMThread::isTerminateRequested() {
	std::unique_lock lock(execution_request_mutex);
	return execution_request == ExecutionRequest::Stop;
}

bool vm::IVMThread::waitForPausedResponse() {
	return std::holds_alternative<api::Paused>(execution_response_queue.pop());
}

bool vm::IVMThread::waitForStoppedResponse() {
	auto response = execution_response_queue.pop();
	return std::holds_alternative<api::ExecutionStopped>(response)
	    || std::holds_alternative<api::ExecutionCompleted>(response)
	    || std::holds_alternative<api::ExecutionPanicked>(response);
}

bool vm::IVMThread::waitForRunningResponse() {
	return std::holds_alternative<api::Running>(execution_response_queue.pop());
}

void vm::IVMThread::respondExecutionRequest(const api::ProcStatus& response) {
	setProcessStatus(response);
	execution_response_queue.push(response);
}

void vm::IVMThread::setProcessStatus(const vm::api::ProcStatus& new_status) {
	setThreadStatus(new_status);
	my_process.setStatus(new_status);
}

bool vm::IVMThread::joinExecutionThread() {
	if (!exec_thread || !exec_thread->joinable()) return false;

	exec_thread->join();
	exec_thread.reset();
	setThreadStatus(api::NotStarted{});
	return true;
}

void vm::IVMThread::safeRun(const std::string& func_name, const RunArguments& run_arguments) {
	try {
		run(func_name, run_arguments);
	} catch (const exceptions::VMRuntimeException& e) {
		std::cerr << "VMThread has panicked: " << e.what() << "\n";
		respondExecutionRequest(api::ExecutionPanicked{ e.what() });
	}
}

bool vm::IVMThread::spawnThreadAndRun(
	const std::string& func_name, const RunArguments& run_arguments
) {
	if (exec_thread)  // There is already a thread running.
		return false;

	exec_thread = std::thread(&IVMThread::safeRun, this, func_name, run_arguments);
	return waitForRunningResponse();
}

void vm::IVMThread::runNoSpawn(const std::string& func_name, const RunArguments& run_arguments) {
	// @TODO: #2040 Make this function check if anyone else is executing anything,
	// or simplify the state checking, perhaps remove state from thread and move all the
	// state to the process?
	safeRun(func_name, run_arguments);
	waitForRunningResponse();
}

bool vm::IVMThread::pause() {
	{
		std::unique_lock lock(execution_request_mutex);

		execution_request              = ExecutionRequest::Pause;
		execution_request_pending_flag = true;
	}

	return waitForPausedResponse();
}

bool vm::IVMThread::resume() {
	{
		std::unique_lock lock(execution_request_mutex);

		execution_request = ExecutionRequest::Resume;
	}
	pause_cv.notify_all();

	return waitForRunningResponse();
}

bool vm::IVMThread::step() {
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

bool vm::IVMThread::stop() {
	{
		std::unique_lock lock(execution_request_mutex);

		execution_request              = ExecutionRequest::Stop;
		execution_request_pending_flag = true;
	}
	pause_cv.notify_all();

	return waitForStoppedResponse();
}

bool vm::IVMThread::hasActiveThread() const { return exec_thread && exec_thread->joinable(); }

void vm::IVMThread::breakActiveExecution() {
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

void vm::IVMThread::notifyPaused() { pause_cv.notify_all(); }

void vm::IVMThread::runDebuggerLoop(std::unique_lock<std::mutex>& lock) {
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
			throw exceptions::VMResumedWithPausedStatusException();
		}
	}
}

void vm::IVMThread::handleBreakpoint() {
	std::unique_lock lock(execution_request_mutex);
	setProcessStatus(api::Paused{});
	execution_request = ExecutionRequest::Pause;
	runDebuggerLoop(lock);
}
