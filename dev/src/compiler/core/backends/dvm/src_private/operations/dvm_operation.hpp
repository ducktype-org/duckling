#pragma once

#include "ctv/ctv.hpp"
#include "dvm_value.hpp"
#include "lir/lir_structure/lir_structure.hpp"

#include "base/collections/optional.hpp"

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
		bool                                 is_extern_c;

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
	struct NoOpOperation {};

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
	 * @brief Represents a simple move operation.
	 */
	struct MoveOperation {
		DVMValue src;
		DVMPlace dest;
	};

	/**
	 * @brief Represents a comparison DVM operation.
	 */
	struct ComparisonOperation {
		OpKind                   op;
		DVMValue                 lhs;
		DVMValue                 rhs;
		base::Optional<DVMPlace> dest;
		// TODOP: Ugly
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
	struct MetaOperation {
		lir::Operation           meta_op;
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
	 * @brief Represents a jump terminator.
	 */
	struct JumpOperation {
		DVMLabel target;
	};

	/**
	 * @brief Represents a branch terminator.
	 */
	struct BranchOperation {
		DVMValue condition;
		DVMLabel true_target;
		DVMLabel false_target;
	};

	/**
	 * @brief Represents a return terminator.
	 */
	struct ReturnOperation {
		base::Optional<DVMValue> value;  /// Empty optional on void returns.
	};

	/**
	 * @brief DVMOperation is a more generalised abstraction over lir::Instruction which allows to
	 * bundle up the instruction lowering logic for similar instructions.
	 *
	 * It works purely in the DVM world working on DVMValues, DVMPlace etc.
	 */
	using DVMOperation = std::variant<
		NoOpOperation,
		UnaryOperation,
		BinaryOperation,
		MoveOperation,
		ComparisonOperation,
		CallOperation,
		AddressOfOperation,
		CastOperation,
		MetaOperation,
		JumpOperation,
		BranchOperation,
		ReturnOperation>;


	/**
	 * @brief Converts a LIR instruction to DVM operation.
	 */
	[[nodiscard]] DVMOperation lirInstrToDVMOperation(
		FunctionLoweringContext& ctx, const lir::Instruction& instr
	);
}
