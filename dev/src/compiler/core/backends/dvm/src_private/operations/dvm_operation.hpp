#pragma once

#include "../dvm_value.hpp"

#include <ctv/ctv.hpp>
#include <lir/lir_structure/lir_structure.hpp>

#include <base/collections/optional.hpp>

#include <vm/bytecode/builders/instruction_builder.hpp>

#include <deque>

namespace compiler::backend_vm::internal {
	using vm::code::builders::OpKind;
	class ProgramLoweringContext;
	class FunctionLoweringContext;

	/**
	 * @brief POD struct storing all needed info for generating a function call in the DVM bytecode
	 * for both LIR and ExternCFunctions.
	 */
	struct FunctionCallInfo {
		DVMCallable                          call_target;
		base::Optional<vm::code::TypeOfData> return_type;
		std::vector<vm::code::TypeOfData>    param_types;

		/**
		 * @brief Created call info for a LIR function.
		 * Translates TSL type layouts to corresponding DVM types.
		 */
		static FunctionCallInfo fromLirFunction(
			const lir::FunctionLiteral& func_literal, ProgramLoweringContext& program_context
		);

		/**
		 * @brief Creates call info for an extern C function.
		 * Translates type names from extern C function signatures to corresponding DVM types.
		 */
		static FunctionCallInfo fromExternCFunction(
			const base::StrID& func_name, ProgramLoweringContext& program_context
		);
	};

	/**
	 * @brief Represents a DVM operation which is a NoOp and is skipped in bytecode lowering.
	 */
	struct NoOperation {};

	/**
	 * @brief Represents a unary DVM operation.
	 */
	struct UnaryOperation {
		OpKind                   op;
		DVMValue                 src;
		base::Optional<DVMPlace> dest;
	};

	/**
	 * @brief Represents a binary DVM operation.
	 */
	struct BinaryOperation {
		OpKind                   op;
		DVMValue                 lhs;
		DVMValue                 rhs;
		base::Optional<DVMPlace> dest;
	};

	/**
	 * @brief Represents a call DVM operation.
	 */
	struct CallOperation {
		FunctionCallInfo         call_info;
		std::deque<DVMValue>     args;
		base::Optional<DVMPlace> dest;
	};

	/**
	 * @brief Represents a call DVM operation.
	 */
	struct BuiltinCallOperation final {
		lir::BuiltinFunctionKind kind;
		std::deque<DVMValue>     args;
		base::Optional<DVMPlace> dest;
	};

	/**
	 * @brief Represents a simple move operation.
	 */
	struct MoveOperation {
		DVMValue src;
		DVMPlace dest;  ///< MoveOperation always has a destination.
	};

	/**
	 * @brief Represents a comparison DVM operation.
	 */
	struct ComparisonOperation {
		OpKind                   op;
		DVMValue                 lhs;
		DVMValue                 rhs;
		base::Optional<DVMPlace> dest;
		// 'lhs_const' and `rhs_const` are needed for lowering comparisons between two immediates to
		// keep the same semantics as in comp-time evaluation.
		base::Optional<ctv::CompileTimeValue> lhs_const;
		base::Optional<ctv::CompileTimeValue> rhs_const;
	};

	/**
	 * @brief Represents an AddressOf DVM operation.
	 */
	struct AddressOfOperation {
		DVMPlace                 src;
		base::Optional<DVMPlace> dest;
	};

	/**
	 * @brief Represents a meta-type operations that require special handling.
	 * These operations don't map directly to DVM opcodes but are lowered
	 * to a series of extern C function calls.
	 */
	struct MetaOperation final {
		lir::MetaKind            meta_kind;
		std::deque<DVMValue>     args;
		base::Optional<DVMPlace> dest;
	};

	/**
	 * @brief Represents a cast operation.
	 * Based on the cast parameters (source type and dest type)
	 * different DVM operations are chosen.
	 */
	struct CastOperation {
		lir::CastParameters      cast_params;
		DVMValue                 src;
		base::Optional<DVMPlace> dest;
	};

	/**
	 * @brief Constructs a variant value: sets the active alternative and stores the payload.
	 */
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): always aggregate-initialized
	struct VariantConstructOperation {
		lir::VariantParameters variant_params;
		/*
		 * Empty when the chosen alternative carries no information (e.g. `()`), as then only
		 * the alternative itself has to be activated.
		 */
		base::Optional<DVMValue> payload;
		DVMPlace                 dest;  ///< The variant place; always present.
	};

	/**
	 * @brief Produces a pointer to the variant's payload, null on alternative mismatch.
	 */
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): always aggregate-initialized
	struct VariantTryProjectOperation {
		lir::VariantParameters variant_params;
		DVMPlace               variant;  ///< A reference to the variant, not the variant itself.
		DVMPlace               dest;     ///< The pointer place; always present.
	};

	/**
	 * @brief Terminator branching on pointer nullness.
	 */
	struct BranchIfNullOperation {
		DVMValue pointer;
		DVMLabel null_target;
		DVMLabel not_null_target;

		std::vector<lir::ScopeFlag> scope_flags;
	};

	/**
	 * @brief Represents a jump terminator.
	 */
	struct JumpOperation {
		DVMLabel target;

		std::vector<lir::ScopeFlag> scope_flags;
	};

	/**
	 * @brief Represents a branch terminator.
	 */
	struct BranchOperation {
		DVMValue condition;
		DVMLabel true_target;
		DVMLabel false_target;

		std::vector<lir::ScopeFlag> scope_flags;
	};

	/**
	 * @brief Represents a return terminator.
	 */
	struct ReturnOperation {
		base::Optional<DVMValue> value;  ///< Empty optional on void returns.

		std::vector<lir::ScopeFlag> scope_flags;
	};

	/**
	 * @brief Represents an unreachable terminator.
	 */
	struct UnreachableOperation {
		std::vector<lir::ScopeFlag> scope_flags;
	};

	/**
	 * @brief DVMOperation is a more generalised abstraction over lir::Instruction which allows to
	 * bundle up the instruction lowering logic for similar instructions.
	 *
	 * It works purely in the DVM world working on DVMValues, DVMPlace etc.
	 */
	using DVMOperation = std::variant<
		NoOperation,
		UnaryOperation,
		BinaryOperation,
		MoveOperation,
		ComparisonOperation,
		CallOperation,
		BuiltinCallOperation,
		AddressOfOperation,
		CastOperation,
		MetaOperation,
		VariantConstructOperation,
		VariantTryProjectOperation,
		JumpOperation,
		BranchOperation,
		BranchIfNullOperation,
		ReturnOperation,
		UnreachableOperation>;


	/**
	 * @brief Converts a LIR instruction to DVM operation.
	 */
	[[nodiscard]] DVMOperation lirInstrToDVMOperation(
		FunctionLoweringContext& ctx, const lir::Instruction& instr
	);
}
