#include "vm.hpp"


#include <vm/api/data/request.hpp>
#include <vm/api/data/response.hpp>
#include <vm/core/process/interface_types.hpp>
#include <vm/core/supervisor/supervisor.hpp>

namespace vm::api {
	void ignoreResponse([[maybe_unused]] const Response& response) {}

	template<class T>
	std::expected<T, ApiError> mapOrWrongResponse(const Response& response) {
		if (std::holds_alternative<T>(response)) return std::get<T>(response);
		return std::unexpected(WrongResponse{});
	}

	template<class T>
	std::expected<T, ApiError> mapOrWrongResponseMove(Response&& response) {
		if (std::holds_alternative<T>(response)) return std::get<T>(std::move(response));
		return std::unexpected(WrongResponse{});
	}

	std::expected<ProcStatus, ApiError> getExecutionStatus(PID pid) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::StatusRequest()))
		    .and_then(mapOrWrongResponse<ProcStatus>);
	}

	std::expected<response::CodePosition, ApiError> pause(PID pid, ThreadID thread_id) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::Pause{ thread_id }))
		    .and_then(mapOrWrongResponse<response::CodePosition>);
	}

	std::expected<response::CodePosition, ApiError> pause(PID pid) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::Pause{ ThreadID{ 0 } }))
		    .and_then(mapOrWrongResponse<response::CodePosition>);
	}

	std::expected<void, ApiError> resume(PID pid, ThreadID thread_id) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::Resume{ thread_id }))
		    .transform(ignoreResponse);
	}

	std::expected<void, ApiError> resume(PID pid) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::Resume{ ThreadID{ 0 } }))
		    .transform(ignoreResponse);
	}

	std::expected<void, ApiError> step(PID pid) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::Step{}))
		    .transform(ignoreResponse);
	}

	std::expected<response::CodePosition, ApiError> waitForBreakpoint(PID pid) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::WaitForBreakpoint{}))
		    .and_then(mapOrWrongResponse<response::CodePosition>);
	}

	std::expected<ProcessInfo, ApiError> spawn() {
		return Supervisor::get().newProcess().transform([](const auto& x) {
			return ProcessInfo{ x };
		});
	}

	std::expected<void, ApiError> loadFiles(PID pid, const std::vector<fs::File>& paths) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::LoadFiles{ paths }))
		    .transform(ignoreResponse);
	}

	std::expected<void, ApiError> loadCode(PID pid, const code::CodeCollection& code) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::LoadCode{ code }))
		    .transform(ignoreResponse);
	}

	std::expected<void, ApiError> run(PID pid, const std::vector<std::string>& args) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::Run{ args }))
		    .transform(ignoreResponse);
	}

	std::expected<ThreadID, ApiError> runFunction(
		PID pid, const std::string& function_name, const FunctionRunArguments& args
	) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(
				pid, request::RunFunction{ .func_name = function_name, .func_args = args }
			))
		    .and_then([](const Response& response) {
				return mapOrWrongResponse<ThreadID>(response);
			});
	}

	std::expected<void, ApiError> join(PID pid, ThreadID thread_id) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::Join{ thread_id }))
		    .transform(ignoreResponse);
	}

	std::expected<ExitValue, ApiError> runFunctionAwait(
		PID pid, const std::string& function_name, const FunctionRunArguments& args
	) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(
				pid, request::RunFunctionAwait{ .func_name = function_name, .func_args = args }
			))
		    .and_then(mapOrWrongResponse<ExitValue>);
	}

	std::expected<void, ApiError> join(PID pid) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::Join{ ThreadID{ 0 } }))
		    .transform(ignoreResponse);
	}

	std::expected<void, ApiError> stop(PID pid) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::Stop{}))
		    .transform(ignoreResponse);
	}

	std::expected<void, ApiError> kill(PID pid) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::Stop{}))
		    .transform(ignoreResponse)
		    .and_then([pid] { return Supervisor::get().killProcess(pid); });
	}

	std::expected<void, ApiError> input(PID pid, const std::string& input) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::Input{ input }))
		    .transform(ignoreResponse);
	}

	std::expected<response::Output, ApiError> output(PID pid) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::Output{}))
		    .and_then(mapOrWrongResponse<response::Output>);
	}

	std::expected<response::Type, ApiError> getType(PID pid, const std::string& type_name) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::TypeMetadata{ type_name }))
		    .and_then(mapOrWrongResponse<response::Type>);
	}

	std::expected<response::VmValue, ApiError> getVmValue(PID pid, const std::string& type_name) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::VmValue{ type_name }))
		    .and_then(mapOrWrongResponseMove<response::VmValue>);
	}

	std::expected<response::NumberOfCurrentStackFrames, ApiError> debuggerGetNumberOfStackFrames(
		PID pid, ThreadID thread_id
	) {
		return Supervisor::get()
		    .doRequest(
				SupervisorRequest(pid, request::DebuggerGetNumberOfCurrentStackFrames{ thread_id })
			)
		    .and_then(mapOrWrongResponse<response::NumberOfCurrentStackFrames>);
	}

	std::expected<response::StackFrameData, ApiError> debuggerGetStackFrameData(
		PID pid, ThreadID thread_id, u64 frame_index
	) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(
				pid,
				request::DebuggerGetStackFrameData{ .thread_id   = thread_id,
		                                            .frame_index = frame_index }
			))
		    .and_then(mapOrWrongResponse<response::StackFrameData>);
	}

	std::expected<void, ApiError> attach(PID pid, std::istream& input, std::ostream& output) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::Attach{ .istream = input, .ostream = output })
		    )
		    .transform(ignoreResponse);
	}

	std::expected<void, ApiError> detach(PID pid) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::Detach{}))
		    .transform(ignoreResponse);
	}

	std::expected<response::CodePosition, ApiError> getCurrentPosition(PID pid) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::ExecutionPosition{}))
		    .and_then(mapOrWrongResponse<response::CodePosition>);
	}

	std::expected<ExitValue, ApiError> getExitValue(PID pid) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::ExitCodeRequest{}))
		    .and_then(mapOrWrongResponse<ExitValue>);
	}

	std::expected<response::Boolean, ApiError> deinitAndValidate(PID pid) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::DeinitAndValidate{}))
		    .and_then(mapOrWrongResponse<response::Boolean>);
	}

	std::expected<void, ApiError> attachStatusListener(
		PID pid, Ref<events::Listener<ProcStatus>> listener
	) {
		return Supervisor::get()
		    .doRequest(SupervisorRequest(pid, request::AttachStatusListener{ .listener = listener }))
		    .transform(ignoreResponse);
	}
}
