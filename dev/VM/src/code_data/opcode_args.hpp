#pragma once

#include <base/string_id.hpp>
#include <base/ints.hpp>
#include <variant>

/**
 * @brief This namespace encapsulates types of opcode arguments.
 * @note All types should be default constructible.
 */
namespace vm::opargs {

	struct ImmediateI64 {
		i64 value = 0;
	};

	struct ImmediateI32 {
		i32 value = 0;
	};

	struct StackLocalI32 {
		i64 offset = 0;
	};

	struct StackLocalI64 {
		i64 offset = 0;
	};

	struct StackLocalPtr {
		i64 offset = 0;
	};

	struct ArgsOffset {
		i64 offset = 0;
	};

	struct Type {
		base::StrID type_name = base::StrID("");
	};

	struct FunctionName {
		base::StrID function_name = base::StrID("");
	};

	struct Label {
		base::StrID label_name = base::StrID("");
	};

	using OpCodeArg = std::variant<
		ImmediateI64,
		ImmediateI32,
		StackLocalI32,
		StackLocalI64,
		StackLocalPtr,
		ArgsOffset,
		Type,
		FunctionName,
		Label>;
}
