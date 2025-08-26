#pragma once

#include "process_info.hpp"

#include <filesystem/file.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/core/process/interface_types.hpp>
#include <vm/core/process/memory/block.hpp>
#include <vm/core/process/memory/pointer.hpp>
#include <vm/core/thread/vmvalue.hpp>

#include <json/json.hpp>

#include <iostream>
#include <string>
#include <vector>

namespace vm::api {
	namespace request {
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

		struct StatusRequest {};

		struct Input {
			std::string input;
		};

		struct Output {};

		struct Attach {
			std::istream& istream;
			std::ostream& ostream;
		};

		struct Detach {};

		struct ExitCodeRequest {};
	}

	using RequestVariant = std::variant<
		request::LoadFiles,
		request::LoadCode,
		request::Pause,
		request::Resume,
		request::Stop,
		request::Run,
		request::RunFunction,
		request::Join,
		request::Step,
		request::WaitForBreakpoint,
		request::ExecutionPosition,
		request::TypeMetadata,
		request::VmValue,
		request::Block,
		request::StatusRequest,
		request::Input,
		request::Output,
		request::Attach,
		request::Detach,
		request::ExitCodeRequest>;

	struct SupervisorRequest {
		PID            pid;
		RequestVariant request;
	};

}
