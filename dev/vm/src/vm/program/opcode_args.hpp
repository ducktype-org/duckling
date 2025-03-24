#pragma once

#include <base/string_id.hpp>
#include <base/ints.hpp>
#include <variant>

/**
 * @brief This namespace encapsulates types of opcode arguments.
 * @note All types should be default constructible.
 */
namespace vm::opargs {

	struct Immediate {
		i64 value = 0;
	};

	struct StackLocalI8 {
		i64 offset = 0;
	};

	struct StackLocalI16 {
		i64 offset = 0;
	};

	struct StackLocalI32 {
		i64 offset = 0;
	};

	struct StackLocalI64 {
		i64 offset = 0;
	};

	struct StackLocalAny {
		i64 offset = 0;
	};

	struct StackLocalPtr {
		i64 offset = 0;
	};

	struct ArgsOffset {
		i64 offset = 0;
	};

#define VM_OPARG_OFFSET_TYPES                                                                \
	StackLocalI8, StackLocalI16, StackLocalI32, StackLocalI64, StackLocalAny, StackLocalPtr, \
		ArgsOffset

	struct Type {
		base::StrID type_name = base::StrID("");
	};

	struct FunctionName {
		base::StrID function_name = base::StrID("");
	};

	struct Label {
		base::StrID label_name = base::StrID("");
	};

#define VM_OPARG_TYPES VM_OPARG_OFFSET_TYPES, Immediate, Type, FunctionName, Label

	using OpCodeArg = std::variant<VM_OPARG_TYPES>;
}
