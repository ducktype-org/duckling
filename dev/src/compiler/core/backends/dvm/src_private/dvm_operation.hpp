#pragma once

#include "lir/lir_structure/lir_structure.hpp"

#include "vm/bytecode/builders/instruction_builder.hpp"

namespace compiler::backend_vm::internal {
	using vm::code::builders::OpKind;

	/**
	 * @brief Represents a simple DVM operation that maps 1:1 to a DVM OpKind.
	 */
	struct SimpleOperation {
		OpKind op;
		bool   operator==(const SimpleOperation& other) const = default;
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

	using DVMOperation = std::variant<SimpleOperation, MetaOperation>;

	/**
	 * @brief Converts a LIR operation to DVM operation.
	 */
	[[nodiscard]] DVMOperation lirOpToDVMOperation(lir::Operation op);


}
