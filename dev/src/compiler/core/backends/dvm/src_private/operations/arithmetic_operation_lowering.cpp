#include "arithmetic_operation_lowering.hpp"

#include "../function_lowering_context.hpp"

namespace compiler::backend_vm::internal {

	void ArithmeticOperationLowerer::lowerUnary(
		FunctionLoweringContext& ctx, const UnaryOperation& unary_operation
	) {
		// If instruction is of the form: a = OP b, then
		// we transform it to:
		// a = b;
		// a = OP a;

		if (unary_operation.dest.isDirect() && unary_operation.dest.is<DVMLocal>()) {
			// If output is a direct place we just use it.
			ctx.storeResult(unary_operation.dest, unary_operation.src);
			ctx.pushInstruction({ unary_operation.op, unary_operation.dest });
		} else {
			// Otherwise it's a global or indirect. We perform the operations on the
			// temporary and then store it in the indirect place.
			auto tmp = ctx.forceToLocal(unary_operation.src);
			ctx.pushInstruction({ unary_operation.op, tmp });
			ctx.storeResult(unary_operation.dest, { tmp, DVMPlace::AccessKind::Direct });
		}
		return;
	}

	void ArithmeticOperationLowerer::lowerBinary(
		FunctionLoweringContext& ctx, const BinaryOperation& binary_operation
	) {
		// In this case we assume we have a very general quadruple of the form:
		// output = arg1 OP arg2;

		// Force globals into locals. Immediates are allowed.
		DVMValue rhs = binary_operation.rhs.is<DVMGlobal>()
		                 ? DVMValue{ ctx.forceToLocal(binary_operation.rhs, "bin_rhs_tmp"),
			                         DVMPlace::AccessKind::Direct }
		                 : binary_operation.rhs;

		if (binary_operation.dest.isDirect() && binary_operation.dest.is<DVMLocal>()) {
			// If instruction is of the form: a = b OP c, then
			// we transform it to:
			// a = b;
			// a = a OP c;
			ctx.storeResult(binary_operation.dest, binary_operation.lhs);
			ctx.pushInstruction({ binary_operation.op, binary_operation.dest, rhs });
		} else {
			// Force globals into locals if needed.
			auto tmp = ctx.forceToLocal(binary_operation.lhs, "bin_tmp");
			ctx.pushInstruction({ binary_operation.op, tmp, rhs });
			ctx.storeResult(binary_operation.dest, { tmp, DVMPlace::AccessKind::Direct });
		}
		return;
	}
}
