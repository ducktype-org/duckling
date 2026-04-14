#include "arithmetic_operation_lowering.hpp"

#include "../function_lowering_context.hpp"

namespace compiler::backend_vm::internal {

	void ArithmeticOperationLowerer::lowerUnary(
		FunctionLoweringContext&    ctx,
		const UnaryOperation&       unary_op,
		const std::deque<DVMValue>& args,
		const DVMPlace&             output
	) {
		CORE_ASSERT(args.size() == 1, "Invalid unary operation argument count");

		// If instruction is of the form: a = OP b, then
		// we transform it to:
		// a = b;
		// a = OP a;

		if (output.isDirect() && output.is<DVMLocal>()) {
			// If output is a direct place we just use it.
			ctx.storeResult(output, args[0]);
			ctx.pushInstruction({ unary_op.op, output });
		} else {
			// Otherwise it's a global or indirect. We perform the operations on the
			// temporary and then store it in the indirect place.
			auto tmp = ctx.forceToLocal(args[0]);
			ctx.pushInstruction({ unary_op.op, tmp });
			ctx.storeResult(output, { tmp, DVMPlace::AccessKind::Direct });
		}
		return;
	}

	void ArithmeticOperationLowerer::lowerBinary(
		FunctionLoweringContext&    ctx,
		const BinaryOperation&      binary_op,
		const std::deque<DVMValue>& args,
		const DVMPlace&             output
	) {
		CORE_ASSERT(args.size() == 2, "Invalid binary operation argument count");
		// In this case we assume we have a very general quadruple of the form:
		// output = arg1 OP arg2;

		// Force globals into locals. Immediates are allowed.
		DVMValue rhs = args[1].is<DVMGlobal>() ? DVMValue{ ctx.forceToLocal(args[1], "bin_rhs_tmp"),
			                                               DVMPlace::AccessKind::Direct }
		                                       : args[1];

		if (output.isDirect() && output.is<DVMLocal>()) {
			// If instruction is of the form: a = b OP c, then
			// we transform it to:
			// a = b;
			// a = a OP c;
			ctx.storeResult(output, args[0]);
			ctx.pushInstruction({ binary_op.op, output, rhs });
		} else {
			// Force globals into locals if needed.
			auto tmp = ctx.forceToLocal(args[0], "bin_tmp");
			ctx.pushInstruction({ binary_op.op, tmp, rhs });
			ctx.storeResult(output, { tmp, DVMPlace::AccessKind::Direct });
		}
		return;
	}
}
