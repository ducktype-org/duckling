#pragma once

#include <base/extend_cpp/variant_match.hpp>
#include <base/types/ints.hpp>

#include <string_id/string_id.hpp>

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

		Immediate(const u64 value): value(value) {}

		u64 value = 0;

		constexpr bool operator==(const Immediate& other) const noexcept {
			return value == other.value;
		}
	};

	DEFINE_STACK_LOCAL(8, "l8");
	DEFINE_STACK_LOCAL(16, "l16");
	DEFINE_STACK_LOCAL(32, "l32");
	DEFINE_STACK_LOCAL(64, "l64");
	DEFINE_STACK_LOCAL(Any, "lany");
	DEFINE_STACK_LOCAL(Ptr, "lptr");
	DEFINE_STACK_LOCAL(Opq, "lopq");
	DEFINE_STACK_LOCAL(Structure, "lste");

	/**
	 * @brief Represents local variant argument.
	 */
	DEFINE_STACK_LOCAL(Vnt, "lvnt");

#define VM_OPARG_LOCAL_TYPES                                                             \
	StackLocal8, StackLocal16, StackLocal32, StackLocal64, StackLocalAny, StackLocalPtr, \
		StackLocalVnt, StackLocalOpq, StackLocalStructure

	DEFINE_GLOBAL(8, "g8");
	DEFINE_GLOBAL(16, "g16");
	DEFINE_GLOBAL(32, "g32");
	DEFINE_GLOBAL(64, "g64");
	DEFINE_GLOBAL(Any, "gany");
	DEFINE_GLOBAL(Ptr, "gptr");
	DEFINE_GLOBAL(Opq, "gopq");
	DEFINE_GLOBAL(Structure, "gste");

	/**
	 * @brief List of all argument types that target global data.
	 */
#define VM_OPARG_GLOBAL_TYPES \
	Global64, Global32, Global16, Global8, GlobalAny, GlobalPtr, GlobalOpq, GlobalStructure

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
	 * @brief Represents field name argument.
	 */
	struct Field final: code::ElementBase {
		static constexpr std::string_view OP_SHORT = "field";

		Field() = default;

		Field(const base::StrID type_name, const base::StrID field_name):
			  type_name(type_name),
			  field_name(field_name) {}

		base::StrID type_name  = base::StrID("");
		base::StrID field_name = base::StrID("");

		constexpr bool operator==(const Field& other) const noexcept {
			return type_name == other.type_name && field_name == other.field_name;
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
		static constexpr std::string_view OP_SHORT = "builtinfunc";

		BuiltinFunctionName() = default;

		BuiltinFunctionName(const base::StrID function_name): function_name(function_name) {}

		base::StrID function_name = base::StrID("");

		constexpr bool operator==(const BuiltinFunctionName& other) const noexcept {
			return function_name == other.function_name;
		}
	};

	/**
	 * @brief Represents extern C function name argument.
	 */
	struct ExtCFunctionName final: code::ElementBase {
		static constexpr std::string_view OP_SHORT = "cfunc";

		ExtCFunctionName() = default;

		ExtCFunctionName(const base::StrID function_name): function_name(function_name) {}

		base::StrID function_name = base::StrID("");

		constexpr bool operator==(const ExtCFunctionName& other) const noexcept {
			return function_name == other.function_name;
		}
	};

	struct MethodName final: code::ElementBase {
		static constexpr std::string_view OP_SHORT = "method";

		MethodName() = default;

		MethodName(const base::StrID method_name): method_name(method_name) {}

		base::StrID method_name = base::StrID("");

		constexpr bool operator==(const MethodName& other) const noexcept {
			return method_name == other.method_name;
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
		Field,
		FunctionName,
		BuiltinFunctionName,
		ExtCFunctionName,
		MethodName,
		Label>;
	using OpCodeArgCRef      = base::CRefifyParams<OpCodeArg>;
	using OpCodeLocalArg     = std::variant<VM_OPARG_LOCAL_TYPES>;
	using OpCodeFunctionArg  = std::variant<FunctionName, BuiltinFunctionName, ExtCFunctionName>;
	using OpCodePrimitiveArg = std::variant<StackLocal8, StackLocal16, StackLocal32, StackLocal64>;
}

#undef DEFINE_STR_ARG_TYPE
#undef DEFINE_STACK_LOCAL
#undef DEFINE_GLOBAL
