#pragma once

#include <vm/code/element_base.hpp>
#include <base/string_id.hpp>
#include <base/ints.hpp>
#include <variant>

/**
 * @brief This namespace encapsulates types of opcode arguments.
 * @note All types should be default constructible.
 */
namespace vm::opargs {

	struct Immediate: code::ElementBase {
		Immediate() = default;

		Immediate(const i64 value): value(value) {}

		i64 value = 0;
	};

	struct StackLocalI8: code::ElementBase {
		StackLocalI8() = default;

		StackLocalI8(const i64 offset): offset(offset) {}

		i64 offset = 0;
	};

	struct StackLocalI16: code::ElementBase {
		StackLocalI16() = default;

		StackLocalI16(const i64 offset): offset(offset) {}

		i64 offset = 0;
	};

	struct StackLocalI32: code::ElementBase {
		StackLocalI32() = default;

		StackLocalI32(const i64 offset): offset(offset) {}

		i64 offset = 0;
	};

	struct StackLocalI64: code::ElementBase {
		StackLocalI64() = default;

		StackLocalI64(const i64 offset): offset(offset) {}

		i64 offset = 0;
	};

	struct StackLocalAny: code::ElementBase {
		StackLocalAny() = default;

		StackLocalAny(const i64 offset): offset(offset) {}

		i64 offset = 0;
	};

	struct StackLocalPtr: code::ElementBase {
		StackLocalPtr() = default;

		StackLocalPtr(const i64 offset): offset(offset) {}

		i64 offset = 0;
	};

	struct ArgsOffset: code::ElementBase {
		ArgsOffset() = default;

		ArgsOffset(const i64 offset): offset(offset) {}

		i64 offset = 0;
	};

#define VM_OPARG_OFFSET_TYPES                                                                \
	StackLocalI8, StackLocalI16, StackLocalI32, StackLocalI64, StackLocalAny, StackLocalPtr, \
		ArgsOffset

	struct Type: code::ElementBase {
		Type() = default;

		Type(const base::StrID type_name): type_name(type_name) {}

		base::StrID type_name = base::StrID("");
	};

	struct FunctionName: code::ElementBase {
		FunctionName() = default;

		FunctionName(const base::StrID function_name): function_name(function_name) {}

		base::StrID function_name = base::StrID("");
	};

	struct Label: code::ElementBase {
		Label() = default;

		Label(base::StrID label_name): label_name(label_name) {}

		base::StrID label_name = base::StrID("");
	};

	using OpCodeArg = std::variant<VM_OPARG_OFFSET_TYPES, Immediate, Type, FunctionName, Label>;
}
