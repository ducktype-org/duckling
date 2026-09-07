#pragma once

#include "status.hpp"
#include "thread_id.hpp"

#include <base/pointers/box.hpp>

#include <diagnostic/source_position.hpp>

#include <vm/core/safe/type_metadata/type.hpp>
#include <vm/core/vmvalue/ivmvalue.hpp>
#include <vm/core/vmvalue/ivmvalueref.hpp>

namespace vm::api {
	namespace response {
		struct Empty final {};

		struct Output final {
			std::string output;
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(Output, output);
		};

		struct Type final {
			TypeCRef type;
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(Type, type);
		};
		
		struct VMValue final {
			Box<::vm::IVMValue> vm_value;
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(VMValue, vm_value);
		};

		struct CodePosition {
			base::StrID                         function_name;
			u64                                 instr_number;
			base::Optional<dia::SourcePosition> source_position;
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(CodePosition, function_name, instr_number);
		};

		struct NumberOfCurrentStackFrames final {
			u64 number_of_stack_frames;
		};

		struct StackFrameData final {
			struct FrameVar final {
				u64                          offset = 0;
				base::Optional<base::StrID>  name;
				base::Optional<base::StrID>  type;
				SharedBox<::vm::IVMValueRef> value;
			};

			base::StrID           function_name;
			std::vector<FrameVar> frame_vars;
		};

		struct ThreadIDs final {
			std::vector<ThreadID> thread_ids;
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(ThreadIDs, thread_ids);
		};

		using Boolean = bool;
	}

	using Response = std::variant<
		ProcStatus,
		response::Output,
		response::Type,
		response::Empty,
		response::CodePosition,
		response::VMValue,
		response::Boolean,
		ThreadID,
		response::NumberOfCurrentStackFrames,
		response::StackFrameData,
		response::ThreadIDs,
		ExitValue>;
}
