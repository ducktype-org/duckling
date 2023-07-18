#pragma once

#include <variant>
#include <filesystem/file.hpp>
#include <memory_data/pointer.hpp>
#include "process_info.hpp"

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
	
		struct TypeMetadata {
			std::string type_name;
		};
		
		struct Block {
			BlockId block_id;
		};
		
		struct Memory {
			Pointer pointer;
			std::uint64_t size;
		};
		
	}
	
	// @Deprecated - ExecutorRequest will have template based api (not variant based)
	using ExecutorRequest = std::variant<
		request::Load,
		request::Resume,
		request::Pause,
		request::Stop,
		request::Run,
		request::Join,
		request::Input,
		request::Output,
		request::Step
	>;
		
	using DataRequest = std::variant<
		request::TypeMetadata,
		request::Block
	>;
	
	struct StatusRequest{};

	using RequestVariant = std::variant<
		ExecutorRequest,
		DataRequest,
		StatusRequest
	>;

	struct SupervisorRequest {
		PID pid;
		RequestVariant request;
	};
	
	SupervisorRequest makeExecutorRequest(PID pid, ExecutorRequest&& data);
	SupervisorRequest makeDataRequest(PID pid, DataRequest&& data);
	SupervisorRequest makeStatusRequest(PID pid);
}
