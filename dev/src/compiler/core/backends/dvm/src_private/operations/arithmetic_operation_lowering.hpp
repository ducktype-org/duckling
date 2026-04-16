#pragma once

namespace compiler::backend_vm::internal {
	// TODOP: Remove the FDs
	class FunctionLoweringContext;
	struct UnaryOperation;
	struct BinaryOperation;

	class ArithmeticOperationLowerer {
	public:
		// TODOP: Doc
		static void lowerUnary(FunctionLoweringContext& ctx, const UnaryOperation& unary_operation);

		// TODOP: Doc
		static void lowerBinary(
			FunctionLoweringContext& ctx, const BinaryOperation& binary_operation
		);
	};

}
