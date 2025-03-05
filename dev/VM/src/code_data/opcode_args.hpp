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

	struct StackLocal64 {
		i64 offset = 0;
	};

	struct StackLocal32 {
		i64 offset = 0;
	};

	struct StackPtr64 {
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

	using OpCodeArg = std::
		variant<ImmediateI64, StackLocal64, StackLocal32, StackPtr64, ArgsOffset, Type, FunctionName, Label>;
}
