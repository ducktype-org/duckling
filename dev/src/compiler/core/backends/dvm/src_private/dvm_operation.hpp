#pragma once

#include "ctv/ctv.hpp"
#include "dvm_value.hpp"
#include "function_lowering_context.hpp"

#include <lir/lir_structure/lir_structure.hpp>

#include "base/collections/optional.hpp"

#include <vm/bytecode/builders/instruction_builder.hpp>

#include <deque>

namespace compiler::backend_vm::internal {
	using vm::code::builders::OpKind;

	// TODOP: Comment about what is the concept of that.

	/**
	 * @brief Represents a DVM operation which is a NoOp and is skipped in bytecode lowering.
	 */
	struct NoOpOperation {};

	/**
	 * @brief Represents a unary DVM operation.
	 */
	struct UnaryOperation {
		OpKind   op;
		DVMValue src;
		DVMPlace dest;  // TODOP: This should be optional
	};

	/**
	 * @brief Represents a binary DVM operation.
	 */
	struct BinaryOperation {
		OpKind   op;
		DVMValue lhs;
		DVMValue rhs;
		DVMPlace dest;
	};

	/**
	 * @brief Represents a comparison DVM operation.
	 */
	struct CallOperation {
		FunctionLoweringContext::FunctionCallInfo call_info;
		std::deque<DVMValue>                      args;
		base::Optional<DVMPlace>                  dest;  // TODOP: Add test for that.
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
		OpKind   op;
		DVMValue lhs;
		DVMValue rhs;
		DVMPlace dest;
		// TODOP: Ugly
		base::Optional<ctv::CompileTimeValue> lhs_const;
		base::Optional<ctv::CompileTimeValue> rhs_const;
	};

	/**
	 * @brief Represents an AddressOf DVM operation.
	 */
	struct AddressOfOperation {
		DVMPlace src;  // TODOP: Comment
		DVMPlace dest;
	};

	/**
	 * @brief Represents a meta-type operation that requires special handling.
	 * These operations don't map directly to DVM opcodes but are lowered
	 * to a series of extern C function calls.
	 */
	struct MetaOperation {
		lir::Operation       meta_op;
		std::deque<DVMValue> args;
		DVMPlace             dest;
	};

	/**
	 * @brief Represents a cast operation.
	 * Based on the cast parameters (source type and dest type)
	 * different DVM operations are chosen.
	 */
	struct CastOperation {
		lir::CastParameters cast_params;
		DVMValue            src;
		DVMPlace            dest;
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
		MetaOperation>;


	/**
	 * @brief Converts a LIR operation to DVM operation.
	 * // TODOP: Better comment.
	 */
	[[nodiscard]] DVMOperation lirInstrToDVMOperation(
		FunctionLoweringContext& ctx, const lir::Instruction& instr
	);


}
