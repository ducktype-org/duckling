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

	// TODOP: Comment about what is the concept of that.

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
	 * @brief Represents a comparison DVM operation.
	 */
	struct CallOperation {
		FunctionCallInfo         call_info;
		std::deque<DVMValue>     args;
		base::Optional<DVMPlace> dest;
	};

	// TODOP: Probably remove.
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
		DVMPlace                 src;  // TODOP: Comment
		base::Optional<DVMPlace> dest;
	};

	/**
	 * @brief Represents a meta-type operation that requires special handling.
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

	struct JumpOperation {
		DVMLabel target;
	};

	struct BranchOperation {
		DVMValue condition;
		DVMLabel true_target;
		DVMLabel false_target;
	};

	struct ReturnOperation {
		base::Optional<DVMValue> value;
	};

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
	 * @brief Converts a LIR operation to DVM operation.
	 * // TODOP: Better comment.
	 */
	[[nodiscard]] DVMOperation lirInstrToDVMOperation(
		FunctionLoweringContext& ctx, const lir::Instruction& instr
	);


}
