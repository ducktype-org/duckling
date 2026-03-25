#pragma once

#include "status.hpp"
#include "thread_id.hpp"

#include <base/pointers/box.hpp>

#include <vm/core/thread/vmvalueref.hpp>

// NOLINTBEGIN(readability-identifier-naming)
template<>
struct nlohmann::adl_serializer<base::RawView> {
	static void to_json(json& j, const base::RawView& e) { j = e.stringView(); }

	static void from_json(const json&, const base::RawView&) {
		CORE_PANIC("Parsing data from JSON into base::RawView is not supported (maybe yet).");
	}
};

template<typename T>
struct nlohmann::adl_serializer<base::Box<T>> {
	static void to_json(json& j, const base::Box<T>& box) { j = *box; }

	static void from_json(const json&, base::Box<T>&) {
		CORE_PANIC("Parsing data from JSON into Box<T> not supported (yet).");
	}
};

// NOLINTEND(readability-identifier-naming)

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
			u64 function_id;
			u64 instr_number;
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(CodePosition, instr_number, function_id);
		};

		struct NumberOfCurrentStackFrames {
			u64 number_of_stack_frames;
		};

		struct StackFrameData {
			struct FrameVar {
				// @todo: #2264 should express name in CodeCollection instead of offset in LowVMProgram
				u64        offset = 0;
				VMValueRef value;
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
