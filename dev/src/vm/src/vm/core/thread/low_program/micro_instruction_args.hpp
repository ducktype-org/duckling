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
 * @brief Convenience wrapper for declaring stack-local micro argument type.
 * @param SUFFIX Name suffix appended to `StackLocal`.
 * @param ARG_SHORT_VALUE Short argument identifier.
 * @param ... Accepted source argument types.
 */
#define DEFINE_MICRO_STACK_LOCAL(SUFFIX, ARG_SHORT_VALUE, ...) \
	DEFINE_MICRO_ARG_TYPE(StackLocal##SUFFIX, ARG_SHORT_VALUE, __VA_ARGS__)

/**
 * @brief Convenience wrapper for declaring global micro argument type.
 * @param SUFFIX Name suffix appended to `Global`.
 * @param ARG_SHORT_VALUE Short argument identifier.
 * @param ... Accepted source argument types.
 */
#define DEFINE_MICRO_GLOBAL(SUFFIX, ARG_SHORT_VALUE, ...) \
	DEFINE_MICRO_ARG_TYPE(Global##SUFFIX, ARG_SHORT_VALUE, __VA_ARGS__)

/**
 * @brief This namespace encapsulates types of micro instruction arguments.
 * @note All types should be default constructible.
 */
namespace vm::low::opargs {

	/**
	 * @brief Stores immediate bits consumed directly by the target instruction.
	 */
	DEFINE_MICRO_ARG_TYPE(Immediate, "imm", vm::opargs::Immediate);

	/** @brief Stores byte offset of 8-bit local on the frame local stack. */
	DEFINE_MICRO_STACK_LOCAL(8, "l8", vm::opargs::StackLocal8);
	/** @brief Stores byte offset of 16-bit local on the frame local stack. */
	DEFINE_MICRO_STACK_LOCAL(16, "l16", vm::opargs::StackLocal16);
	/** @brief Stores byte offset of 32-bit local on the frame local stack. */
	DEFINE_MICRO_STACK_LOCAL(32, "l32", vm::opargs::StackLocal32);
	/** @brief Stores byte offset of 64-bit local on the frame local stack. */
	DEFINE_MICRO_STACK_LOCAL(64, "l64", vm::opargs::StackLocal64);
	/** @brief Stores byte offset of local Pointer value on the frame local stack. */
	DEFINE_MICRO_STACK_LOCAL(Ptr, "lptr", vm::opargs::StackLocalPtr);
	/** @brief Stores byte offset of local opaque value on the frame local stack. */
	DEFINE_MICRO_STACK_LOCAL(Opq, "lopq", vm::opargs::StackLocalOpq);

#define VM_MICRO_INSTR_ARG_LOCAL_STACK_TYPES \
	StackLocal8, StackLocal16, StackLocal32, StackLocal64, StackLocalPtr, StackLocalOpq

	/** @brief Stores index of type-erased local data in the frame local block reference stack. */
	DEFINE_MICRO_ARG_TYPE(BlockStackLocalAny, "blany", vm::opargs::StackLocalAny);
	/** @brief Stores index of local struct storage in Frame::block_ref_stack (not a byte offset). */
	DEFINE_MICRO_ARG_TYPE(BlockStackLocalStructure, "blste", vm::opargs::StackLocalStructure);
	/** @brief Stores index of local variant storage in Frame::block_ref_stack (not a byte offset). */
	DEFINE_MICRO_ARG_TYPE(BlockStackLocalVariant, "blvnt", vm::opargs::StackLocalVnt);

#define VM_MICRO_INSTR_ARG_LOCAL_BLOCK_STACK_TYPES \
	BlockStackLocalAny, BlockStackLocalStructure, BlockStackLocalVariant

	/** @brief Stores ID/index of 8-bit global variable in LowVMProgram globals map. */
	DEFINE_MICRO_GLOBAL(8, "g8", vm::opargs::Global8);
	/** @brief Stores ID/index of 16-bit global variable in LowVMProgram globals map. */
	DEFINE_MICRO_GLOBAL(16, "g16", vm::opargs::Global16);
	/** @brief Stores ID/index of 32-bit global variable in LowVMProgram globals map. */
	DEFINE_MICRO_GLOBAL(32, "g32", vm::opargs::Global32);
	/** @brief Stores ID/index of 64-bit global variable in LowVMProgram globals map. */
	DEFINE_MICRO_GLOBAL(64, "g64", vm::opargs::Global64);
	/** @brief Stores ID/index of global variable of any type in LowVMProgram globals map. */
	DEFINE_MICRO_GLOBAL(Any, "gany", vm::opargs::GlobalAny);
	/** @brief Stores ID/index of global Pointer value in LowVMProgram globals map. */
	DEFINE_MICRO_GLOBAL(Ptr, "gptr", vm::opargs::GlobalPtr);
	/** @brief Stores ID/index of global opaque value in LowVMProgram globals map. */
	DEFINE_MICRO_GLOBAL(Opq, "gopq", vm::opargs::GlobalOpq);
	/** @brief Stores ID/index of global struct storage in LowVMProgram globals map. */
	DEFINE_MICRO_GLOBAL(Structure, "gste", vm::opargs::GlobalStructure);

#define VM_MICRO_INSTR_ARG_GLOBAL_TYPES \
	Global64, Global32, Global16, Global8, GlobalPtr, GlobalAny, GlobalOpq, GlobalStructure

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
		VM_MICRO_INSTR_ARG_LOCAL_STACK_TYPES,
		VM_MICRO_INSTR_ARG_LOCAL_BLOCK_STACK_TYPES,
		VM_MICRO_INSTR_ARG_GLOBAL_TYPES,
		Immediate,
		Type,
		Field,
		FunctionID,
		BuiltinFunctionID,
		ExtCFunction,
		MethodName,
		Label>;
	using InstructionArgCRef = base::CRefifyParams<InstructionArg>;

	using InstructionLocalStackArg      = std::variant<VM_MICRO_INSTR_ARG_LOCAL_STACK_TYPES>;
	using InstructionLocalBlockStackArg = std::variant<VM_MICRO_INSTR_ARG_LOCAL_BLOCK_STACK_TYPES>;
	using InstructionGlobalArg          = std::variant<VM_MICRO_INSTR_ARG_GLOBAL_TYPES>;

	using InstructionFunctionArg = std::variant<FunctionID, BuiltinFunctionID, ExtCFunction>;
	using InstructionPrimitiveArg
		= std::variant<StackLocal8, StackLocal16, StackLocal32, StackLocal64>;

	template<typename T>
	concept ArgumentType = base::IsVariantMember<T, InstructionArg>;

	template<typename T>
	concept LocalStackArgumentType = base::IsVariantMember<T, InstructionLocalStackArg>;
	template<typename T>
	concept LocalBlockStackArgumentType = base::IsVariantMember<T, InstructionLocalBlockStackArg>;
	template<typename T>
	concept GlobalArgumentType = base::IsVariantMember<T, InstructionGlobalArg>;
}

#undef DEFINE_MICRO_ARG_TYPE
#undef DEFINE_MICRO_STACK_LOCAL
#undef DEFINE_MICRO_GLOBAL
