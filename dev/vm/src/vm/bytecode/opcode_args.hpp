#pragma once

#include <base/ints.hpp>
#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <vm/bytecode/element_base.hpp>

#include <string_view>
#include <variant>

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

	/**
	 * @brief Represents `l8` argument.
	 */
	struct StackLocalI8 final: code::ElementBase {
		static constexpr std::string_view OP_SHORT = "l8";

		StackLocalI8() = default;

		StackLocalI8(const base::StrID var_name): var_name(var_name) {}

		base::StrID var_name;

		constexpr bool operator==(const StackLocalI8& other) const noexcept {
			return var_name == other.var_name;
		}
	};

	/**
	 * @brief Represents `l16` argument.
	 */
	struct StackLocalI16 final: code::ElementBase {
		static constexpr std::string_view OP_SHORT = "l16";

		StackLocalI16() = default;

		StackLocalI16(const base::StrID var_name): var_name(var_name) {}

		base::StrID var_name;

		constexpr bool operator==(const StackLocalI16& other) const noexcept {
			return var_name == other.var_name;
		}
	};

	/**
	 * @brief Represents `l32` argument.
	 */
	struct StackLocalI32 final: code::ElementBase {
		static constexpr std::string_view OP_SHORT = "l32";

		StackLocalI32() = default;

		StackLocalI32(const base::StrID var_name): var_name(var_name) {}

		base::StrID var_name;

		constexpr bool operator==(const StackLocalI32& other) const noexcept {
			return var_name == other.var_name;
		}
	};

	/**
	 * @brief Represents `l64` argument.
	 */
	struct StackLocalI64 final: code::ElementBase {
		static constexpr std::string_view OP_SHORT = "l64";

		StackLocalI64() = default;

		StackLocalI64(const base::StrID var_name): var_name(var_name) {}

		base::StrID var_name;

		constexpr bool operator==(const StackLocalI64& other) const noexcept {
			return var_name == other.var_name;
		}
	};

	/**
	 * @brief Represents `any` argument.
	 */
	struct StackLocalAny final: code::ElementBase {
		static constexpr std::string_view OP_SHORT = "lany";

		StackLocalAny() = default;

		StackLocalAny(const base::StrID var_name): var_name(var_name) {}

		base::StrID var_name;

		constexpr bool operator==(const StackLocalAny& other) const noexcept {
			return var_name == other.var_name;
		}
	};

	/**
	 * @brief Represents `lptr` argument.
	 */
	struct StackLocalPtr final: code::ElementBase {
		static constexpr std::string_view OP_SHORT = "lptr";
		StackLocalPtr()                            = default;

		StackLocalPtr(const base::StrID var_name): var_name(var_name) {}

		base::StrID var_name;

		constexpr bool operator==(const StackLocalPtr& other) const noexcept {
			return var_name == other.var_name;
		}
	};

	/**
	 * @brief List of all argument types that target stack offset.
	 */
#define VM_OPARG_LOCAL_TYPES \
	StackLocalI8, StackLocalI16, StackLocalI32, StackLocalI64, StackLocalAny, StackLocalPtr

	/**
	 * @brief Represents `g64` - global i64 argument.
	 */
	struct GlobalI64 final: code::ElementBase {
		static constexpr std::string_view OP_SHORT = "g64";

		GlobalI64() = default;

		GlobalI64(const base::StrID global_data_name): global_data_name(global_data_name) {}

		base::StrID global_data_name = base::StrID("");

		constexpr bool operator==(const GlobalI64& other) const noexcept {
			return global_data_name == other.global_data_name;
		}
	};

	/**
	 * @brief Represents `g32` - global i32 argument.
	 */
	struct GlobalI32 final: code::ElementBase {
		static constexpr std::string_view OP_SHORT = "g32";

		GlobalI32() = default;

		GlobalI32(const base::StrID global_data_name): global_data_name(global_data_name) {}

		base::StrID global_data_name = base::StrID("");

		constexpr bool operator==(const GlobalI32& other) const noexcept {
			return global_data_name == other.global_data_name;
		}
	};

	/**
	 * @brief Represents `g16` - global i16 argument.
	 */
	struct GlobalI16 final: code::ElementBase {
		static constexpr std::string_view OP_SHORT = "g16";

		GlobalI16() = default;

		GlobalI16(const base::StrID global_data_name): global_data_name(global_data_name) {}

		base::StrID global_data_name = base::StrID("");

		constexpr bool operator==(const GlobalI16& other) const noexcept {
			return global_data_name == other.global_data_name;
		}
	};

	/**
	 * @brief Represents `g8` - global i8 argument.
	 */
	struct GlobalI8 final: code::ElementBase {
		static constexpr std::string_view OP_SHORT = "g8";

		GlobalI8() = default;

		GlobalI8(const base::StrID global_data_name): global_data_name(global_data_name) {}

		base::StrID global_data_name = base::StrID("");

		constexpr bool operator==(const GlobalI8& other) const noexcept {
			return global_data_name == other.global_data_name;
		}
	};

	/**
	 * @brief Represents `gptr` - global ptr argument.
	 */
	struct GlobalPtr final: code::ElementBase {
		static constexpr std::string_view OP_SHORT = "gptr";

		GlobalPtr() = default;

		GlobalPtr(const base::StrID global_data_name): global_data_name(global_data_name) {}

		base::StrID global_data_name = base::StrID("");

		constexpr bool operator==(const GlobalPtr& other) const noexcept {
			return global_data_name == other.global_data_name;
		}
	};

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
	using OpCodeLocalArg = std::variant<VM_OPARG_LOCAL_TYPES>;
	using OpCodeFunctionArg = std::variant<FunctionName, BuiltinFunctionName>;
}
