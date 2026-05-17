#pragma once

#include "process_info.hpp"

#include <events/emitter.hpp>

#include <filesystem/file.hpp>

#include <vm/api/data/response.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/core/process/interface_types.hpp>
#include <vm/core/safe/memory/pointer.hpp>
#include <vm/core/vmvalue/vmvalue.hpp>

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

		struct Pause {
			ThreadID thread_id;
		};

		struct Resume {
			ThreadID thread_id;
		};

		struct Stop {};

		struct Run {
			ProgramRunArguments program_args;
		};

		struct RunFunction {
			std::string          func_name;
			FunctionRunArguments func_args;
		};

		struct Join {
			ThreadID thread_id;
		};

		struct RunFunctionAwait {
			std::string          func_name;
			FunctionRunArguments func_args;
		};

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

		struct DebuggerGetNumberOfCurrentStackFrames {
			ThreadID thread_id;
		};

		struct DebuggerGetStackFrameData {
			ThreadID thread_id;
			u64      frame_index;
		};

		struct AttachStatusListener {
			Ref<events::Listener<ProcStatus>> listener;
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
		request::RunFunctionAwait,
		request::Join,
		request::Step,
		request::WaitForBreakpoint,
		request::ExecutionPosition,
		request::TypeMetadata,
		request::VmValue,
		request::StatusRequest,
		request::DebuggerGetNumberOfCurrentStackFrames,
		request::DebuggerGetStackFrameData,
		request::Input,
		request::Output,
		request::Attach,
		request::Detach,
		request::ExitCodeRequest,
		request::DeinitAndValidate,
		request::AttachStatusListener>;

	struct SupervisorRequest {
		PID            pid;
		RequestVariant request;
	};

}
