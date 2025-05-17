#pragma once

#include <base/ints.hpp>
#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/element_base.hpp>

#include <string_view>
#include <variant>

#define DEFINE_STR_ARG_TYPE(NAME, FIELD_NAME, OP_SHORT_VALUE)         \
	struct NAME final: code::ElementBase {                            \
		static constexpr std::string_view OP_SHORT = OP_SHORT_VALUE;  \
		NAME()                                     = default;         \
		NAME(const base::StrID FIELD_NAME): FIELD_NAME(FIELD_NAME) {} \
		base::StrID    FIELD_NAME;                                    \
		constexpr bool operator==(const NAME& other) const noexcept { \
			return FIELD_NAME == other.FIELD_NAME;                    \
		}                                                             \
	}

#define DEFINE_STACK_LOCAL(SUFFIX, OP_SHORT_VALUE) \
	DEFINE_STR_ARG_TYPE(StackLocal##SUFFIX, var_name, OP_SHORT_VALUE)
#define DEFINE_GLOBAL(SUFFIX, OP_SHORT_VALUE) \
	DEFINE_STR_ARG_TYPE(Global##SUFFIX, global_data_name, OP_SHORT_VALUE)

/**
 * @brief This namespace encapsulates types of opcode arguments.
 * @note All types should be default constructible.
 */
namespace vm::opargs {

	/**
	 * @brief Represents `imm` argument.
	 */
	struct Immediate final: code::ElementBase {
		static constexpr std::string_view OP_SHORT = "imm";

		Immediate() = default;

		Immediate(const i64 value): value(value) {}

		i64 value = 0;

		constexpr bool operator==(const Immediate& other) const noexcept {
			return value == other.value;
		}
	};

	DEFINE_STACK_LOCAL(I8, "l8");
	DEFINE_STACK_LOCAL(I16, "l16");
	DEFINE_STACK_LOCAL(I32, "l32");
	DEFINE_STACK_LOCAL(I64, "l64");
	DEFINE_STACK_LOCAL(Any, "lany");
	DEFINE_STACK_LOCAL(Ptr, "lptr");
	DEFINE_STACK_LOCAL(Variant, "lvnt");

#define VM_OPARG_LOCAL_TYPES                                                                 \
	StackLocalI8, StackLocalI16, StackLocalI32, StackLocalI64, StackLocalAny, StackLocalPtr, \
		StackLocalVariant

	DEFINE_GLOBAL(I8, "g8");
	DEFINE_GLOBAL(I16, "g16");
	DEFINE_GLOBAL(I32, "g32");
	DEFINE_GLOBAL(I64, "g64");
	DEFINE_GLOBAL(Ptr, "gptr");

	/**
	 * @brief List of all argument types that target global data.
	 */
#define VM_OPARG_GLOBAL_TYPES GlobalI64, GlobalI32, GlobalI16, GlobalI8, GlobalPtr

	/**
	 * @brief Represents type name argument.
	 */
	struct Type final: code::ElementBase {
		static constexpr std::string_view OP_SHORT = "type";

		Type() = default;

		Type(const base::StrID type_name): type_name(type_name) {}

		base::StrID type_name = base::StrID("");

		constexpr bool operator==(const Type& other) const noexcept {
			return type_name == other.type_name;
		}
	};

	/**
	 * @brief Represents function name argument.
	 */
	struct FunctionName final: code::ElementBase {
		static constexpr std::string_view OP_SHORT = "func";

		FunctionName() = default;

		FunctionName(const base::StrID function_name): function_name(function_name) {}

		base::StrID function_name = base::StrID("");

		constexpr bool operator==(const FunctionName& other) const noexcept {
			return function_name == other.function_name;
		}
	};

	struct BuiltinFunctionName final: code::ElementBase {
		static constexpr std::string_view OP_SHORT = "builtin_func";

		BuiltinFunctionName() = default;

		BuiltinFunctionName(const base::StrID function_name): function_name(function_name) {}

		base::StrID function_name = base::StrID("");

		constexpr bool operator==(const BuiltinFunctionName& other) const noexcept {
			return function_name == other.function_name;
		}
	};

	/**
	 * @brief Represents label name argument.
	 */
	struct Label final: code::ElementBase {
		static constexpr std::string_view OP_SHORT = "label";

		Label() = default;

		Label(base::StrID label_name): label_name(label_name) {}

		base::StrID label_name = base::StrID("");

		constexpr bool operator==(const Label& other) const noexcept {
			return label_name == other.label_name;
		}
	};

	/**
	 * @brief Storage class for any kind of opcode argument.
	 */
	using OpCodeArg = std::variant<
		VM_OPARG_LOCAL_TYPES,
		VM_OPARG_GLOBAL_TYPES,
		Immediate,
		Type,
		FunctionName,
		BuiltinFunctionName,
		Label>;
	using OpCodeLocalArg    = std::variant<VM_OPARG_LOCAL_TYPES>;
	using OpCodeFunctionArg = std::variant<FunctionName, BuiltinFunctionName>;
}

#undef DEFINE_STR_ARG_TYPE
#undef DEFINE_STACK_LOCAL
#undef DEFINE_GLOBAL
