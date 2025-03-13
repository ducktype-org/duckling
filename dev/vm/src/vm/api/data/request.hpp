#pragma once

#include "process_info.hpp"

#include <filesystem/file.hpp>
#include <variant>
#include <vm/core/process/memory/block.hpp>
#include <vm/core/process/memory/pointer.hpp>

namespace vm::api {
	namespace request {
		struct Load {
			fs::FilePath filename;
		};

		struct Pause {};

		struct Resume {};

		struct Stop {};

		struct Run {};

		struct Input {
			std::string input;
		};

		struct Output {};

		struct Join {};

		struct Step {};

		struct WaitForBreakpoint {};

		struct ExecutionPosition {};

		struct TypeMetadata {
			std::string type_name;
		};

		struct Block {
			BlockID block_id;
		};

		struct Memory {
			Pointer pointer;
			u64     size{};
		};

		struct Attach {
			std::istream& istream;
			std::ostream& ostream;
		};

		struct Detach {};

	}

	// @Deprecated - ExecutorRequest will have template based api (not variant based)
	using ExecutorRequest = std::variant<
		request::Load,
		request::Resume,
		request::Pause,
		request::Stop,
		request::Run,
		request::Join,
		request::Step,
		request::WaitForBreakpoint,
		request::ExecutionPosition>;

	using IORequest
		= std::variant<request::Input, request::Output, request::Attach, request::Detach>;

	using DataRequest = std::variant<request::TypeMetadata, request::Block>;

	struct StatusRequest {};

	using RequestVariant = std::variant<ExecutorRequest, DataRequest, StatusRequest, IORequest>;

	struct SupervisorRequest {
		PID            pid;
		RequestVariant request;
	};

	SupervisorRequest makeExecutorRequest(PID pid, ExecutorRequest&& data);
	SupervisorRequest makeDataRequest(PID pid, DataRequest&& data);
	SupervisorRequest makeStatusRequest(PID pid);
	SupervisorRequest makeIORequest(PID pid, IORequest&& data);
}
