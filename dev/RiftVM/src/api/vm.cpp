#include "vm.hpp"

#include "supervisor/supervisor.hpp"

namespace vm::api {
	void ignoreResponse([[maybe_unused]] const Response& response){};

	template<class T>
	result<T, ApiError> mapOrWrongResponse(const Response& response) {
		if (std::holds_alternative<T>(response)) {
			return std::get<T>(response);
		}
		return failure(WrongResponse{});
	}

	result<VCPUStatus, ApiError> getExecutionStatus(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeStatusRequest(pid))
		    .flat_map(mapOrWrongResponse<VCPUStatus>);
	}

	result<void, ApiError> pause(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Pause{}))
		    .map(ignoreResponse);
	}

	result<void, ApiError> resume(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Resume{}))
		    .map(ignoreResponse);
	}

	result<void, ApiError> step(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Step{}))
		    .map(ignoreResponse);
	}

	result<ProcessInfo, ApiError> spawn(bool usesStdio) {
		return Supervisor::get().newProcess(usesStdio).map([](auto& x) { return ProcessInfo(x); });
	}

	result<void, ApiError> loadFile(PID pid, const fs::FilePath& path) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Load{ path }))
		    .map(ignoreResponse);
	}

	result<void, ApiError> run(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Run{}))
		    .map(ignoreResponse);
	}

	result<void, ApiError> join(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Join{}))
		    .map(ignoreResponse);
	}

	result<void, ApiError> stop(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Stop{}))
		    .map(ignoreResponse);
	}

	result<void, ApiError> kill(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Stop{}))
		    .map(ignoreResponse)
		    .flat_map([pid] { return Supervisor::get().killProcess(pid); });
	}

	result<void, ApiError> input(PID pid, const std::string& input) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Input{ input }))
		    .map(ignoreResponse);
	}

	result<response::Output, ApiError> output(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Output{}))
		    .flat_map(mapOrWrongResponse<response::Output>);
	}

	result<TypeCRef, ApiError> getType(PID pid, const std::string& type_name) {
		return Supervisor::get()
		    .doRequest(api::makeDataRequest(pid, request::TypeMetadata{ type_name }))
		    .flat_map(mapOrWrongResponse<TypeCRef>);
	}

	result<response::Block, ApiError> getBlock(PID pid, u64 block_id) {
		return Supervisor::get()
		    .doRequest(api::makeDataRequest(pid, request::Block{ BlockId(block_id) }))
		    .flat_map(mapOrWrongResponse<response::Block>);
	}
}
