#pragma once

#include "execution_config.hpp"
#include "process_info.hpp"

#include <events/emitter.hpp>

#include <filesystem/file.hpp>

#include <vm/api/data/response.hpp>
#include <vm/bytecode/bytecode.hpp>
#include <vm/core/process/interface_types.hpp>
#include <vm/core/vmvalue/ivmvalue.hpp>

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

		struct RunAwait {
			ProgramRunArguments program_args;
		};

		struct RunFunction {
			std::string          func_name;
			FunctionRunArguments func_args;
		};

		struct RunFunctionAwait {
			std::string          func_name;
			FunctionRunArguments func_args;
		};

		struct ExecRuntimeExpr {
			ThreadID       thread_id;
			code::Function expr;
		};

		struct ExecRuntimeExprFromFile {
			ThreadID thread_id;
			fs::File file;
		};

		struct Join {
			ThreadID thread_id;
		};

		struct Step {
			ThreadID thread_id;
		};

		struct PauseAll {};

		struct WaitForBreakpoint {
			ThreadID thread_id;
		};

		struct ExecutionPosition {
			base::Optional<usize> frame_idx;
		};

		struct TypeMetadata {
			std::string type_name;
		};

		struct VMValue {
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

		struct SetExecutionConfig {
			api::ExecutionConfig config;
		};

		struct AttachOutputListener {
			Ref<events::Listener<std::string>> listener;
		};

		struct SetBreakpoint {
			base::StrID function_name;
			u64         instruction_index;
			bool        enable;
		};

		struct MapFileLineToCodeCollectionPosition {
			fs::File file;
			u64      line_number;
		};
	}

	using RequestVariant = std::variant<
		request::LoadFiles,
		request::LoadCode,
		request::Pause,
		request::PauseAll,
		request::Resume,
		request::Stop,
		request::Run,
		request::RunAwait,
		request::RunFunction,
		request::RunFunctionAwait,
		request::Join,
		request::Step,
		request::WaitForBreakpoint,
		request::ExecutionPosition,
		request::TypeMetadata,
		request::VMValue,
		request::StatusRequest,
		request::DebuggerGetNumberOfCurrentStackFrames,
		request::DebuggerGetStackFrameData,
		request::Input,
		request::Output,
		request::Attach,
		request::Detach,
		request::ExitCodeRequest,
		request::DeinitAndValidate,
		request::AttachStatusListener,
		request::SetExecutionConfig,
		request::AttachOutputListener,
		request::SetBreakpoint,
		request::MapFileLineToCodeCollectionPosition,
		request::ExecRuntimeExpr,
		request::ExecRuntimeExprFromFile>;
}
