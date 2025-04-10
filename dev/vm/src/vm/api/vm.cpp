#include "vm.hpp"

#include <vm/api/data/request.hpp>
#include <vm/api/data/response.hpp>
#include <vm/core/supervisor/supervisor.hpp>

namespace vm::api {
	void ignoreResponse([[maybe_unused]] const Response& response) {}

	template<class T>
	std::expected<T, ApiError> mapOrWrongResponse(const Response& response) {
		if (std::holds_alternative<T>(response)) return std::get<T>(response);
		return std::unexpected(WrongResponse{});
	}

	std::expected<ProcStatus, ApiError> getExecutionStatus(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeStatusRequest(pid))
		    .and_then(mapOrWrongResponse<ProcStatus>);
	}

	std::expected<response::CodePosition, ApiError> pause(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Pause{}))
		    .and_then(mapOrWrongResponse<response::CodePosition>);
	}

	std::expected<void, ApiError> resume(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Resume{}))
		    .transform(ignoreResponse);
	}

	std::expected<void, ApiError> step(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Step{}))
		    .transform(ignoreResponse);
	}

	std::expected<response::CodePosition, ApiError> waitForBreakpoint(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::WaitForBreakpoint{}))
		    .and_then(mapOrWrongResponse<response::CodePosition>);
	}

	std::expected<ProcessInfo, ApiError> spawn() {
		return Supervisor::get().newProcess().transform([](const auto& x) {
			return ProcessInfo{ x };
		});
	}

	std::expected<void, ApiError> loadFile(PID pid, const fs::FilePath& path) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Load{ path }))
		    .transform(ignoreResponse);
	}

	std::expected<void, ApiError> run(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Run{}))
		    .transform(ignoreResponse);
	}

	std::expected<void, ApiError> join(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Join{}))
		    .transform(ignoreResponse);
	}

	std::expected<void, ApiError> stop(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Stop{}))
		    .transform(ignoreResponse);
	}

	std::expected<void, ApiError> kill(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::Stop{}))
		    .transform(ignoreResponse)
		    .and_then([pid] { return Supervisor::get().killProcess(pid); });
	}

	std::expected<void, ApiError> input(PID pid, const std::string& input) {
		return Supervisor::get()
		    .doRequest(api::makeIORequest(pid, request::Input{ input }))
		    .transform(ignoreResponse);
	}

	std::expected<response::Output, ApiError> output(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeIORequest(pid, request::Output{}))
		    .and_then(mapOrWrongResponse<response::Output>);
	}

	std::expected<TypeCRef, ApiError> getType(PID pid, const std::string& type_name) {
		return Supervisor::get()
		    .doRequest(api::makeDataRequest(pid, request::TypeMetadata{ type_name }))
		    .and_then(mapOrWrongResponse<TypeCRef>);
	}

	std::expected<response::Block, ApiError> getBlock(PID pid, u64 block_id) {
		return Supervisor::get()
		    .doRequest(api::makeDataRequest(pid, request::Block{ BlockID(block_id) }))
		    .and_then(mapOrWrongResponse<response::Block>);
	}

	std::expected<void, ApiError> attach(PID pid, std::istream& input, std::ostream& output) {
		return Supervisor::get()
		    .doRequest(api::makeIORequest(pid, request::Attach{ input, output }))
		    .transform(ignoreResponse);
	}

	std::expected<void, ApiError> detach(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeIORequest(pid, request::Detach{}))
		    .transform(ignoreResponse);
	}

	std::expected<response::CodePosition, ApiError> getCurrentPosition(PID pid) {
		return Supervisor::get()
		    .doRequest(api::makeExecutorRequest(pid, request::ExecutionPosition{}))
		    .and_then(mapOrWrongResponse<response::CodePosition>);
	}
}
