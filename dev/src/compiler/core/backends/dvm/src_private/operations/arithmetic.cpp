// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../function_lowering_context.hpp"
#include "dvm_operation.hpp"
#include "instruction_lowerer.hpp"

namespace compiler::backend_vm::internal {

	void InstructionLowerer::lower(const MoveOperation& op) {
		// Otherwise, it's a simple assignment.
		ctx->maybeStoreResult(op.dest, op.src);
	}

	void InstructionLowerer::lower(const UnaryOperation& op) {
		// If instruction is of the form: a = OP b, then
		// we transform it to:
		// a = b;
		// a = OP a;

		if (op.dest && op.dest->isDirect()) {
			// If output is a direct place we just use it.
			ctx->maybeStoreResult(op.dest, op.src);
			ctx->pushInstruction({ op.op, *op.dest });
		} else {
			// Otherwise it's indirect. We perform the operations on a temporary and then store it
			// in the indirect place. The temporary must be a fresh copy: the opcode mutates it in
			// place, and reusing op.src directly would clobber a live local.
			auto tmp = ctx->copyToTempPlace(op.src, "un_tmp");
			ctx->pushInstruction({ op.op, tmp });
			ctx->maybeStoreResult(op.dest, { tmp });
		}
	}

	void InstructionLowerer::lower(const BinaryOperation& op) {
		// In this case we assume we have a very general quadruple of the form:
		// output = arg1 OP arg2;

		auto is_different_from_arg = [](const DVMPlace& place, const DVMValue& arg) -> bool {
			// We keep place.isDirect() check here as well, even though it's present in the condition
			// below, as this is logically needed, to ensure that place is different from arg.
			return place.isDirect() && !(DVMValue{ place } == arg);
		};

		if (op.dest && op.dest->isDirect() && is_different_from_arg(*op.dest, op.lhs)
		    && is_different_from_arg(*op.dest, op.rhs)) {
			// If instruction is of the form: a = b OP c and a is different than b and c, then
			// we transform it to:
			// a = b;
			// a = a OP c;
			ctx->maybeStoreResult(op.dest, op.lhs);
			ctx->pushInstruction({ op.op, *op.dest, op.rhs });
		} else {
			// If the output is accessed through a pointer, or aliases one of the arguments,
			// or doesn't exist, we perform the operation on a temporary. The temporary must
			// be a fresh copy: the opcode mutates it in place, and reusing op.lhs directly
			// would clobber a live local.
			auto tmp = ctx->copyToTempPlace(op.lhs, "bin_tmp");
			ctx->pushInstruction({ op.op, tmp, op.rhs });
			ctx->maybeStoreResult(op.dest, { tmp });
		}
	}
}
