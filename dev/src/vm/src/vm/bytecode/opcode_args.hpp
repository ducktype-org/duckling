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

#define DEFINE_PLACE(SUFFIX, OP_SHORT_VALUE) \
	DEFINE_STR_ARG_TYPE(Place##SUFFIX, var_name, OP_SHORT_VALUE)

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

	DEFINE_PLACE(8, "p8");
	DEFINE_PLACE(16, "p16");
	DEFINE_PLACE(32, "p32");
	DEFINE_PLACE(64, "p64");
	DEFINE_PLACE(Any, "pany");
	DEFINE_PLACE(Ptr, "pptr");
	DEFINE_PLACE(Opq, "popq");
	DEFINE_PLACE(Structure, "pste");
	DEFINE_PLACE(FSTable, "pfst");

	/**
	 * @brief Represents place variant argument.
	 */
	DEFINE_PLACE(Vnt, "pvnt");

#define VM_OPARG_PLACE_TYPES                                                                   \
	Place8, Place16, Place32, Place64, PlaceAny, PlacePtr, PlaceVnt, PlaceOpq, PlaceStructure, \
		PlaceFSTable

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

	/**
	 * @brief Represents FFI function name argument.
	 */
	struct FFIFunctionName final: code::ElementBase {
		static constexpr std::string_view OP_SHORT = "ffifunc";

		FFIFunctionName() = default;

		FFIFunctionName(const base::StrID function_name): function_name(function_name) {}

		base::StrID function_name = base::StrID("");

		constexpr bool operator==(const FFIFunctionName& other) const noexcept {
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
		VM_OPARG_PLACE_TYPES,
		Immediate,
		Type,
		Field,
		FunctionName,
		BuiltinFunctionName,
		ExtCFunctionName,
		FFIFunctionName,
		MethodName,
		Label>;
	using OpCodeArgCRef  = base::CRefifyParams<OpCodeArg>;
	using OpCodePlaceArg = std::variant<VM_OPARG_PLACE_TYPES>;
	using OpCodeFunctionArg
		= std::variant<FunctionName, BuiltinFunctionName, ExtCFunctionName, FFIFunctionName>;
	using OpCodePrimitiveArg = std::variant<Place8, Place16, Place32, Place64>;

	template<typename T>
	concept ArgumentType = base::IsVariantMember<T, OpCodeArg>;

	template<typename T>
	concept PlaceArgumentType = base::IsVariantMember<T, OpCodePlaceArg>;
}

#undef DEFINE_STR_ARG_TYPE
#undef DEFINE_PLACE
