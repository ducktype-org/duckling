#pragma once

#include "process_info.hpp"

#include <filesystem/file.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/core/process/interface_types.hpp>
#include <vm/core/process/memory/block.hpp>
#include <vm/core/process/memory/pointer.hpp>
#include <vm/core/thread/vmvalue.hpp>

#include <variant>
#include <vector>

namespace vm::api {
	namespace request {
		struct LoadStdlib {};

		struct LoadFiles {
			std::vector<fs::File> filenames;
		};

		struct LoadCode {
			std::vector<code::CodeCollection> code_collections;
		};

		struct Pause {};

		struct Resume {};

		struct Stop {};

		struct Run {
			ProgramRunArguments program_args;
		};

		struct RunFunction {
			std::string          func_name;
			FunctionRunArguments func_args;
		};

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

		struct VmValue {
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
		request::LoadStdlib,
		request::LoadFiles,
		request::LoadCode,
		request::Resume,
		request::Pause,
		request::Stop,
		request::Run,
		request::RunFunction,
		request::Join,
		request::Step,
		request::WaitForBreakpoint,
		request::ExecutionPosition>;

	using IORequest
		= std::variant<request::Input, request::Output, request::Attach, request::Detach>;

	using DataRequest = std::variant<request::TypeMetadata, request::Block, request::VmValue>;

	struct StatusRequest {};

	struct ExitCodeRequest {};

	using RequestVariant
		= std::variant<ExecutorRequest, DataRequest, StatusRequest, IORequest, ExitCodeRequest>;

	struct SupervisorRequest {
		PID            pid;
		RequestVariant request;
	};

	SupervisorRequest makeExecutorRequest(PID pid, ExecutorRequest&& data);
	SupervisorRequest makeDataRequest(PID pid, DataRequest&& data);
	SupervisorRequest makeStatusRequest(PID pid);
	SupervisorRequest makeExitCodeRequest(PID pid);
	SupervisorRequest makeIORequest(PID pid, IORequest&& data);
}
