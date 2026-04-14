#pragma once

#include <lir/lir_structure/lir_structure.hpp>

#include <vm/bytecode/builders/instruction_builder.hpp>

namespace compiler::backend_vm::internal {
	using vm::code::builders::OpKind;

	/**
	 * @brief Represents a DVM operation which is a NoOp and is skipped in bytecode lowering.
	 */
	struct NoOpOperation {};

	/**
	 * @brief Represents a unary DVM operation.
	 */
	struct UnaryOperation {
		OpKind op;
	};

	/**
	 * @brief Represents a binary DVM operation.
	 */
	struct BinaryOperation {
		OpKind op;
	};

	/**
	 * @brief Represents a comparison DVM operation.
	 */
	struct CallOperation {};

	struct MoveOperation {};  // TODOP: Probably remove.

	/**
	 * @brief Represents a comparison DVM operation.
	 */
	struct ComparisonOperation {
		OpKind op;
	};

	/**
	 * @brief Represents an AddressOf DVM operation.
	 */
	struct AddressOfOperation {
		OpKind op;
	};

	/**
	 * @brief Represents a meta-type operation that requires special handling.
	 * These operations don't map directly to DVM opcodes but are lowered
	 * to a series of extern C function calls.
	 */
	struct MetaOperation {
		lir::Operation meta_op;
		bool           operator==(const MetaOperation& other) const = default;
	};

	/**
	 * @brief Represents a cast operation.
	 * Based on the cast parameters (source type and dest type)
	 * different DVM operations are chosen.
	 */
	struct CastOperation {
		lir::CastParameters cast_params;
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
	 */
	[[nodiscard]] DVMOperation lirInstrToDVMOperation(const lir::Instruction& instr);


}
