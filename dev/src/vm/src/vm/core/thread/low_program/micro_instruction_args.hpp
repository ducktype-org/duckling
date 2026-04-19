#pragma once

#include <base/comptime/type_traits.hpp>
#include <base/preproc/for_each.hpp>
#include <base/types/ints.hpp>

#include <vm/bytecode/opcode_args.hpp>

#include <string_view>
#include <tuple>
#include <variant>

#define ASSERT_GOOD_SOURCE(FROM)                                                                  \
	static_assert(                                                                                \
		std::constructible_from<u64, FROM> || base::IsVariantMember<FROM, vm::opargs::OpCodeArg>, \
		"Type cannot be used as a source argument"                                                \
	);

/**
 * @brief Declares a micro instruction argument type.
 *
 * Generated type stores lowered runtime value in `value`, exposes short textual
 * identifier in `ARG_SHORT`, and declares accepted source argument types through
 * `ConstructibleFrom`.
 *
 * @param NAME Type name to generate.
 * @param ARG_SHORT_VALUE Short argument identifier (for diagnostics/stringification).
 * @param ... Variadic list of high-level source argument types accepted by lowering.
 */
#define DEFINE_MICRO_ARG_TYPE(NAME, ARG_SHORT_VALUE, ...)                      \
	struct NAME final {                                                        \
		static constexpr std::string_view ARG_SHORT = ARG_SHORT_VALUE;         \
                                                                               \
		using ConstructibleFrom = std::tuple<__VA_ARGS__>;                     \
		FOR_EACH(ASSERT_GOOD_SOURCE, __VA_ARGS__)                              \
                                                                               \
		u64 value        = 0;                                                  \
		constexpr NAME() = default;                                            \
		constexpr explicit NAME(const u64 value): value(value) {}              \
		constexpr bool operator==(const NAME& other) const noexcept = default; \
	}

/**
 * @brief This namespace encapsulates types of micro instruction arguments.
 * @note All types should be default constructible.
 */
namespace vm::low::opargs {

	/**
	 * @brief Stores immediate bits consumed directly by the target instruction.
	 */
	DEFINE_MICRO_ARG_TYPE(Immediate, "imm", vm::opargs::Immediate);

	/**
	 * The Place types store the location of the data (either on the local stack or in the global
	 * buffer) used by the instruction. The encoding uses the highest bit of the value to
	 * distinguish between local (0) or global (1) value.
	 */
	/** @brief Stores byte offset of 8-bit local on the frame local stack or the global buffer. */
	DEFINE_MICRO_ARG_TYPE(Place8, "p8", vm::opargs::Place8);
	/** @brief Stores byte offset of 16-bit local on the frame local stack or the global buffer. */
	DEFINE_MICRO_ARG_TYPE(Place16, "p16", vm::opargs::Place16);
	/** @brief Stores byte offset of 32-bit local on the frame local stack or the global buffer. */
	DEFINE_MICRO_ARG_TYPE(Place32, "p32", vm::opargs::Place32);
	/** @brief Stores byte offset of 64-bit local on the frame local stack or the global buffer. */
	DEFINE_MICRO_ARG_TYPE(Place64, "p64", vm::opargs::Place64);
	/** @brief Stores byte offset of local Pointer value on the frame local stack or the global
	 * buffer. */
	DEFINE_MICRO_ARG_TYPE(PlacePtr, "pptr", vm::opargs::PlacePtr);
	/** @brief Stores byte offset of local opaque value on the frame local stack or the global
	 * buffer. */
	DEFINE_MICRO_ARG_TYPE(PlaceOpq, "popq", vm::opargs::PlaceOpq);

#define VM_MICRO_INSTR_ARG_PLACE_OFFSET_TYPES Place8, Place16, Place32, Place64, PlacePtr, PlaceOpq

	/**
	 * The PlaceBlock types store the index of the block reference for globals and locals.
	 * The encoding uses the highest bit of the value to distinguish
	 * between local (0) or global (1) block reference.
	 */
	/** @brief Stores index of type-erased local data in the frame local block reference stack or
	 * the global blocks buffer. */
	DEFINE_MICRO_ARG_TYPE(PlaceBlockAny, "bany", vm::opargs::PlaceAny, vm::opargs::PlaceVnt);
	/** @brief Stores index of local struct storage in Frame::block_ref_stack (not a byte offset) or
	 * the global blocks buffer. */
	DEFINE_MICRO_ARG_TYPE(PlaceBlockStructure, "bste", vm::opargs::PlaceStructure);
	/** @brief Stores index of local variant storage in Frame::block_ref_stack (not a byte offset)
	 * or the global blocks buffer. */
	DEFINE_MICRO_ARG_TYPE(PlaceBlockVariant, "bvnt", vm::opargs::PlaceVnt);

#define VM_MICRO_INSTR_ARG_BLOCK_PLACE_TYPES PlaceBlockAny, PlaceBlockStructure, PlaceBlockVariant

	/** @brief Stores TypeCRef (pointer) from type metadata. */
	DEFINE_MICRO_ARG_TYPE(Type, "type", vm::opargs::Type);
	/** @brief Stores byte offset of a field within its containing type layout. */
	DEFINE_MICRO_ARG_TYPE(Field, "field", vm::opargs::Field);
	/** @brief Stores function ID from LowVMProgram functions map. */
	DEFINE_MICRO_ARG_TYPE(FunctionID, "func", vm::opargs::FunctionName);
	/** @brief Stores underlying numeric value of builtins::BuiltinFunctionID. */
	DEFINE_MICRO_ARG_TYPE(BuiltinFunctionID, "builtinfunc", vm::opargs::BuiltinFunctionName);
	/** @brief Stores extern C function pointer in LowVMProgram extern C functions map. */
	DEFINE_MICRO_ARG_TYPE(ExtCFunction, "cfunc", vm::opargs::ExtCFunctionName);
	/** @brief Stores lowered method identifier used for virtual dispatch lookup. */
	DEFINE_MICRO_ARG_TYPE(MethodName, "method", vm::opargs::MethodName);
	/** @brief Stores relative instruction jump offset after label linking. */
	DEFINE_MICRO_ARG_TYPE(Label, "label", vm::opargs::Label);

	/**
	 * @brief Storage class for any kind of micro instruction argument.
	 */
	using InstructionArg = std::variant<
		VM_MICRO_INSTR_ARG_PLACE_OFFSET_TYPES,
		VM_MICRO_INSTR_ARG_BLOCK_PLACE_TYPES,
		Immediate,
		Type,
		Field,
		FunctionID,
		BuiltinFunctionID,
		ExtCFunction,
		MethodName,
		Label>;
	using InstructionArgCRef = base::CRefifyParams<InstructionArg>;

	using InstructionPlaceDataArg  = std::variant<VM_MICRO_INSTR_ARG_PLACE_OFFSET_TYPES>;
	using InstructionPlaceBlockArg = std::variant<VM_MICRO_INSTR_ARG_BLOCK_PLACE_TYPES>;

	using InstructionFunctionArg = std::variant<FunctionID, BuiltinFunctionID, ExtCFunction>;

	template<typename T>
	concept ArgumentType = base::IsVariantMember<T, InstructionArg>;

	template<typename T>
	concept PlaceDataArgumentType = base::IsVariantMember<T, InstructionPlaceDataArg>;
	template<typename T>
	concept PlaceBlockArgumentType = base::IsVariantMember<T, InstructionPlaceBlockArg>;
}

#undef DEFINE_MICRO_ARG_TYPE
#undef DEFINE_MICRO_STACK_LOCAL
#undef DEFINE_MICRO_GLOBAL
