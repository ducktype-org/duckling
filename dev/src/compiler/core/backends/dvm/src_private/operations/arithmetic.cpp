#include "../function_lowering_context.hpp"
#include "instruction_lowerer.hpp"
#include "operations/dvm_operation.hpp"

namespace compiler::backend_vm::internal {

	void InstructionLowerer::lower(const MoveOperation& op) {
		// Otherwise, it's a simple assignment.
		ctx->storeResult(op.dest, op.src);
	}

	void InstructionLowerer::lower(const UnaryOperation& op) {
		// If instruction is of the form: a = OP b, then
		// we transform it to:
		// a = b;
		// a = OP a;

		if (op.dest && op.dest->isDirect() && op.dest->is<DVMLocal>()) {
			// If output is a direct place we just use it.
			ctx->storeResult(op.dest, op.src);
			ctx->pushInstruction({ op.op, *op.dest });
		} else {
			// Otherwise it's a global or indirect. We perform the operations on the
			// temporary and then store it in the indirect place.
			auto tmp = ctx->forceToLocal(op.src);
			ctx->pushInstruction({ op.op, tmp });
			ctx->storeResult(op.dest, { tmp, DVMPlace::AccessKind::Direct });
		}
	}

	void InstructionLowerer::lower(const BinaryOperation& op) {
		// In this case we assume we have a very general quadruple of the form:
		// output = arg1 OP arg2;

		// Force globals into locals. Immediates are allowed.
		DVMValue rhs = op.rhs.is<DVMGlobal>() ? DVMValue{ ctx->forceToLocal(op.rhs, "bin_rhs_tmp"),
			                                              DVMPlace::AccessKind::Direct }
		                                      : op.rhs;

		if (op.dest && op.dest->isDirect() && op.dest->is<DVMLocal>()) {
			// If instruction is of the form: a = b OP c, then
			// we transform it to:
			// a = b;
			// a = a OP c;
			ctx->storeResult(op.dest, op.lhs);
			ctx->pushInstruction({ op.op, *op.dest, rhs });
		} else {
			// Force globals into locals if needed.
			auto tmp = ctx->forceToLocal(op.lhs, "bin_tmp");
			ctx->pushInstruction({ op.op, tmp, rhs });
			ctx->storeResult(op.dest, { tmp, DVMPlace::AccessKind::Direct });
		}
	}
}
