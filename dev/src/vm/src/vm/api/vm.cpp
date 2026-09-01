#include "vm.hpp"

#include <vm/api/data/request.hpp>
#include <vm/api/data/response.hpp>
#include <vm/api/data/thread_id.hpp>
#include <vm/core/process/interface_types.hpp>
#include <vm/core/supervisor/supervisor.hpp>

#define REQUEST(VALUE) Supervisor::get().doRequest(pid, api::request::VALUE)

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
		return REQUEST(StatusRequest()).and_then(mapOrWrongResponse<ProcStatus>);
	}

	std::expected<response::CodePosition, ApiError> pause(PID pid, ThreadID thread_id) {
		return REQUEST(Pause{ thread_id }).and_then(mapOrWrongResponse<response::CodePosition>);
	}

	std::expected<void, ApiError> resume(PID pid, ThreadID thread_id) {
		return REQUEST(Resume{ thread_id }).transform(ignoreResponse);
	}

	std::expected<void, ApiError> step(PID pid, ThreadID thread_id) {
		return REQUEST(Step{ thread_id }).transform(ignoreResponse);
	}

	std::expected<response::ThreadIDs, ApiError> pauseAll(PID pid) {
		return REQUEST(PauseAll{}).and_then(mapOrWrongResponse<response::ThreadIDs>);
	}

	std::expected<response::CodePosition, ApiError> waitForBreakpoint(PID pid, ThreadID thread_id) {
		return REQUEST(WaitForBreakpoint{ thread_id })
		    .and_then(mapOrWrongResponse<response::CodePosition>);
	}

	std::expected<ProcessInfo, ApiError> spawn(const ProcessConfig& options) {
		return Supervisor::get().newProcess(options).transform([](const auto& x) {
			return ProcessInfo{ x };
		});
	}

	std::expected<void, ApiError> loadFiles(PID pid, const std::vector<fs::File>& paths) {
		return REQUEST(LoadFiles{ paths }).transform(ignoreResponse);
	}

	std::expected<void, ApiError> loadCode(PID pid, const code::CodeCollection& code) {
		return REQUEST(LoadCode{ code }).transform(ignoreResponse);
	}

	std::expected<void, ApiError> run(PID pid, const std::vector<std::string>& args) {
		return REQUEST(Run{ args }).transform(ignoreResponse);
	}

	std::expected<ExitValue, ApiError> runAwait(PID pid, const std::vector<std::string>& args) {
		return REQUEST(RunAwait{ args }).and_then(mapOrWrongResponse<ExitValue>);
	}

	std::expected<ThreadID, ApiError> runFunction(
		PID pid, const std::string& function_name, const FunctionRunArguments& args
	) {
		return REQUEST(RunFunction{ .func_name = function_name COMMA.func_args = args })
		    .and_then(mapOrWrongResponse<ThreadID>);
	}

	std::expected<void, ApiError> join(PID pid, ThreadID thread_id) {
		return REQUEST(Join{ thread_id }).transform(ignoreResponse);
	}

	std::expected<ExitValue, ApiError> runFunctionAwait(
		PID pid, const std::string& function_name, const FunctionRunArguments& args
	) {
		return REQUEST(RunFunctionAwait{ .func_name = function_name COMMA.func_args = args })
		    .and_then(mapOrWrongResponse<ExitValue>);
	}

	std::expected<void, ApiError> setExecutionConfig(PID pid, ExecutionConfig config) {
		return REQUEST(SetExecutionConfig{ config }).transform(ignoreResponse);
	}

	std::expected<void, ApiError> stop(PID pid) {
		return REQUEST(Stop{}).transform(ignoreResponse);
	}

	std::expected<void, ApiError> kill(PID pid) {
		return REQUEST(Stop{}).transform(ignoreResponse).and_then([pid] {
			return Supervisor::get().killProcess(pid);
		});
	}

	std::expected<void, ApiError> input(PID pid, const std::string& input) {
		return REQUEST(Input{ input }).transform(ignoreResponse);
	}

	std::expected<response::Output, ApiError> output(PID pid) {
		return REQUEST(Output{}).and_then(mapOrWrongResponse<response::Output>);
	}

	std::expected<response::Type, ApiError> getType(PID pid, const std::string& type_name) {
		return REQUEST(TypeMetadata{ type_name }).and_then(mapOrWrongResponse<response::Type>);
	}

	std::expected<response::VMValue, ApiError> getVMValue(PID pid, const std::string& type_name) {
		return REQUEST(VMValue{ type_name }).and_then(mapOrWrongResponseMove<response::VMValue>);
	}

	std::expected<response::NumberOfCurrentStackFrames, ApiError> debuggerGetNumberOfStackFrames(
		PID pid, ThreadID thread_id
	) {
		return REQUEST(DebuggerGetNumberOfCurrentStackFrames{ thread_id })
		    .and_then(mapOrWrongResponse<response::NumberOfCurrentStackFrames>);
	}

	std::expected<response::StackFrameData, ApiError> debuggerGetStackFrameData(
		PID pid, ThreadID thread_id, u64 frame_index
	) {
		return Supervisor::get()
		    .doRequest(
				pid,
				request::DebuggerGetStackFrameData{ .thread_id   = thread_id,
		                                            .frame_index = frame_index }
			)
		    .and_then(mapOrWrongResponse<response::StackFrameData>);
	}

	std::expected<void, ApiError> attach(PID pid, std::istream& input, std::ostream& output) {
		return REQUEST(Attach{ .istream = input COMMA.ostream = output }).transform(ignoreResponse);
	}

	std::expected<void, ApiError> detach(PID pid) {
		return REQUEST(Detach{}).transform(ignoreResponse);
	}

	std::expected<response::CodePosition, ApiError> getCurrentPosition(
		PID pid, base::Optional<usize> frame_idx
	) {
		return REQUEST(ExecutionPosition{ frame_idx })
		    .and_then(mapOrWrongResponse<response::CodePosition>);
	}

	std::expected<ExitValue, ApiError> getExitValue(PID pid) {
		return REQUEST(ExitCodeRequest{}).and_then(mapOrWrongResponse<ExitValue>);
	}

	std::expected<response::Boolean, ApiError> deinitAndValidate(PID pid) {
		return REQUEST(DeinitAndValidate{}).and_then(mapOrWrongResponse<response::Boolean>);
	}

	std::expected<void, ApiError> attachStatusListener(
		PID pid, Ref<events::Listener<ProcStatus>> listener
	) {
		return REQUEST(AttachStatusListener{ .listener = listener }).transform(ignoreResponse);
	}

	std::expected<void, ApiError> attachOutputListener(
		PID pid, Ref<events::Listener<std::string>> listener
	) {
		return REQUEST(AttachOutputListener{ .listener = listener }).transform(ignoreResponse);
	}

	std::expected<void, ApiError> setBreakpoint(
		PID pid, base::StrID function_name, u64 instruction_index, bool enable
	) {
		return Supervisor::get()
		    .doRequest(
				pid,
				request::SetBreakpoint{ .function_name     = function_name,
		                                .instruction_index = instruction_index,
		                                .enable            = enable }
			)
		    .transform(ignoreResponse);
	}

	std::expected<response::CodePosition, ApiError> mapFileLineToCodeCollectionPosition(
		PID pid, fs::File file, usize line_number
	) {
		return REQUEST(MapFileLineToCodeCollectionPosition{
						   .file = std::move(file) COMMA.line_number = line_number })
		    .and_then(mapOrWrongResponse<response::CodePosition>);
	}
}
