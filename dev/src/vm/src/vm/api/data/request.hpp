#pragma once

#include "process_info.hpp"

#include <filesystem/file.hpp>

#include <vm/bytecode/bytecode.hpp>
#include <vm/core/process/interface_types.hpp>
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
			code::CodeCollection code_collection;
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

		struct DeinitAndValidate {};

		struct DebuggerGetNumberOfCurrentStackFrames {};

		struct DebuggerGetStackFrameVars {
			u64 frame_index;
		};

		struct DebuggerGetPointerData {
			Pointer pointer;
			u64     size;
		};

		struct DebuggerDereferencePointer {
			Pointer pointer;
		};

		struct DebuggerLoadFiles {
			std::vector<fs::File> filenames;
		};

		struct DebuggerPutBreakpoint {
			u64 function_id;
			u64 instr_number;
		};

		struct DebuggerRemoveBreakpoint {
			u64 function_id;
			u64 instr_number;
		};
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
		request::StatusRequest,
		request::DebuggerGetNumberOfCurrentStackFrames,
		request::DebuggerGetStackFrameVars,
		request::DebuggerGetPointerData,
		request::DebuggerDereferencePointer,
		request::DebuggerLoadFiles,
		request::DebuggerPutBreakpoint,
		request::DebuggerRemoveBreakpoint,
		request::Input,
		request::Output,
		request::Attach,
		request::Detach,
		request::ExitCodeRequest,
		request::DeinitAndValidate>;

	struct SupervisorRequest {
		PID            pid;
		RequestVariant request;
	};

}
