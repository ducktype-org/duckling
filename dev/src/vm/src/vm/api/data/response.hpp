#pragma once

#include "status.hpp"
#include "thread_id.hpp"

#include <base/pointers/box.hpp>

#include <diagnostic/source_position.hpp>

#include <vm/core/vmvalue/vmvalueref.hpp>

namespace vm::api {
	namespace response {
		struct Empty {};

		struct Output {
			std::string output;
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(Output, output);
		};

		struct Type {
			TypeCRef type;
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(Type, type);
		};

		struct VmValue {
			Box<::vm::VmValue> vm_value;
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(VmValue, vm_value);
		};

		struct CodePosition {
			base::StrID                         function_name;
			u64                                 instr_number;
			base::Optional<dia::SourcePosition> source_position;
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(CodePosition, function_name, instr_number);
		};

		struct NumberOfCurrentStackFrames {
			u64 number_of_stack_frames;
		};

		struct StackFrameData {
			struct FrameVar {
				u64         offset = 0;
				base::Optional<base::StrID> name;
				base::Optional<base::StrID> type;
				VMValueRef  value;
			};

			base::StrID           function_name;
			std::vector<FrameVar> frame_vars;
		};

		using Boolean = bool;
	}

	using Response = std::variant<
		ProcStatus,
		response::Output,
		response::Type,
		response::Empty,
		response::CodePosition,
		response::VmValue,
		response::Boolean,
		ThreadID,
		response::NumberOfCurrentStackFrames,
		response::StackFrameData,
		ExitValue>;
}
