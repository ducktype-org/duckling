#include "vm.hpp"
#include "api/data/request.hpp"
#include <core/supervisor/supervisor.hpp>

namespace vm::api {
	void ignoreResponse([[maybe_unused]] const Response& response){};

	template<class T>
	cpp::result<T, ApiError> mapOrWrongResponse(const Response& response) {
		if (std::holds_alternative<T>(response)) return std::get<T>(response);
		return cpp::failure(WrongResponse{});
	}

	cpp::result<ProcStatus, ApiError> getExecutionStatus(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeStatusRequest(pid))
		    .flat_map(mapOrWrongResponse<ProcStatus>);
	}

	cpp::result<void, ApiError> pause(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Pause{}))
		    .map(ignoreResponse);
	}

	cpp::result<void, ApiError> resume(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Resume{}))
		    .map(ignoreResponse);
	}

	cpp::result<void, ApiError> step(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Step{}))
		    .map(ignoreResponse);
	}

	cpp::result<ProcessInfo, ApiError> spawn() {
		return Supervisor::get().newProcess().map([](auto& x) { return ProcessInfo{ x }; });
	}

	cpp::result<void, ApiError> loadFile(PID pid, const fs::FilePath& path) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Load{ path }))
		    .map(ignoreResponse);
	}

	cpp::result<void, ApiError> run(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Run{}))
		    .map(ignoreResponse);
	}

	cpp::result<void, ApiError> join(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Join{}))
		    .map(ignoreResponse);
	}

	cpp::result<void, ApiError> stop(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Stop{}))
		    .map(ignoreResponse);
	}

	cpp::result<void, ApiError> kill(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Stop{}))
		    .map(ignoreResponse)
		    .flat_map([pid] { return Supervisor::get().killProcess(pid); });
	}

	cpp::result<void, ApiError> input(PID pid, const std::string& input) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Input{ input }))
		    .map(ignoreResponse);
	}

	cpp::result<response::Output, ApiError> output(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Output{}))
		    .flat_map(mapOrWrongResponse<response::Output>);
	}

	cpp::result<TypeCRef, ApiError> getType(PID pid, const std::string& type_name) {
		return Supervisor::get()
		    .doRequest(api::makeDataRequest(pid, request::TypeMetadata{ type_name }))
		    .flat_map(mapOrWrongResponse<TypeCRef>);
	}

	cpp::result<response::Block, ApiError> getBlock(PID pid, u64 block_id) {
		return Supervisor::get()
		    .doRequest(api::makeDataRequest(pid, request::Block{ BlockID(block_id) }))
		    .flat_map(mapOrWrongResponse<response::Block>);
	}

	cpp::result<void, ApiError> attach(PID pid, std::istream& input, std::ostream& output) {
		return Supervisor::get()
		    .doRequest(api::makeIORequest(pid, request::Attach{ input, output }))
		    .map(ignoreResponse);
	}

	cpp::result<void, ApiError> detach(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeIORequest(pid, request::Detach{}))
		    .map(ignoreResponse);
	}
}
