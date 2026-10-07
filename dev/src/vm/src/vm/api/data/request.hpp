// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
		struct LoadFiles final {
			std::vector<fs::File> filenames;
		};

		struct LoadCode final {
			code::CodeCollection code_collection;
		};

		struct Pause final {
			ThreadID thread_id;
		};

		struct Resume final {
			ThreadID thread_id;
		};

		struct Stop final {};

		struct Run final {
			ProgramRunArguments program_args;
		};

		struct RunAwait final {
			ProgramRunArguments program_args;
		};

		struct RunFunction final {
			std::string          func_name;
			FunctionRunArguments func_args;
		};

		struct RunFunctionAwait final {
			std::string          func_name;
			FunctionRunArguments func_args;
		};

		struct Join final {
			ThreadID thread_id;
		};

		struct Step final {
			ThreadID thread_id;
		};

		struct PauseAll final {};

		struct WaitForBreakpoint final {
			ThreadID thread_id;
		};

		struct ExecutionPosition final {
			base::Optional<usize> frame_idx;
		};

		struct TypeMetadata final {
			std::string type_name;
		};

		struct VMValue final {
			std::string type_name;
		};

		struct StatusRequest final {};

		struct Input final {
			std::string input;
		};

		struct Output final {};

		struct Attach final {
			std::istream& istream;
			std::ostream& ostream;
		};

		struct Detach final {};

		struct ExitCodeRequest final {};

		struct DeinitAndValidate final {};

		struct DebuggerGetNumberOfCurrentStackFrames final {
			ThreadID thread_id;
		};

		struct DebuggerGetStackFrameData final {
			ThreadID thread_id;
			u64      frame_index;
		};

		struct AttachStatusListener final {
			Ref<events::Listener<ProcStatus>> listener;
		};

		struct SetExecutionConfig final {
			api::ExecutionConfig config;
		};

		struct AttachOutputListener final {
			Ref<events::Listener<std::string>> listener;
		};

		struct SetBreakpoint final {
			base::StrID function_name;
			u64         instruction_index;
			bool        enable;
		};

		struct MapFileLineToCodeCollectionPosition final {
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
		request::MapFileLineToCodeCollectionPosition>;
}
