#pragma once

#include "vm/program/element_base.hpp"
#include <base/string_id.hpp>
#include <base/ints.hpp>
#include <variant>

/**
 * @brief This namespace encapsulates types of opcode arguments.
 * @note All types should be default constructible.
 */
namespace vm::opargs {

	struct Immediate: program::ElementBase {
		Immediate() = default;

		Immediate(i64 value): value(value) {}

		i64 value = 0;
	};

	struct StackLocalI8: program::ElementBase {
		StackLocalI8() = default;

		StackLocalI8(i64 offset): offset(offset) {}

		i64 offset = 0;
	};

	struct StackLocalI16: program::ElementBase {
		StackLocalI16() = default;

		StackLocalI16(i64 offset): offset(offset) {}

		i64 offset = 0;
	};

	struct StackLocalI32: program::ElementBase {
		StackLocalI32() = default;

		StackLocalI32(i64 offset): offset(offset) {}

		i64 offset = 0;
	};

	struct StackLocalI64: program::ElementBase {
		StackLocalI64() = default;

		StackLocalI64(i64 offset): offset(offset) {}

		i64 offset = 0;
	};

	struct StackLocalAny: program::ElementBase {
		StackLocalAny() = default;

		StackLocalAny(i64 offset): offset(offset) {}

		i64 offset = 0;
	};

	struct StackLocalPtr: program::ElementBase {
		StackLocalPtr() = default;

		StackLocalPtr(i64 offset): offset(offset) {}

		i64 offset = 0;
	};

	struct ArgsOffset: program::ElementBase {
		ArgsOffset() = default;

		ArgsOffset(i64 offset): offset(offset) {}

		i64 offset = 0;
	};

#define VM_OPARG_OFFSET_TYPES                                                                \
	StackLocalI8, StackLocalI16, StackLocalI32, StackLocalI64, StackLocalAny, StackLocalPtr, \
		ArgsOffset

	struct Type: program::ElementBase {
		Type() = default;

		Type(base::StrID type_name): type_name(type_name) {}

		base::StrID type_name = base::StrID("");
	};

	struct FunctionName: program::ElementBase {
		FunctionName() = default;

		FunctionName(base::StrID function_name): function_name(function_name) {}

		base::StrID function_name = base::StrID("");
	};

	struct Label: program::ElementBase {
		Label() = default;

		Label(base::StrID label_name): label_name(label_name) {}

		base::StrID label_name = base::StrID("");
	};

	using OpCodeArg = std::variant<VM_OPARG_OFFSET_TYPES, Immediate, Type, FunctionName, Label>;
}
