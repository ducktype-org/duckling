#pragma once

#include "../dvm_operation.hpp"
#include "../dvm_value.hpp"

namespace compiler::backend_vm::internal {
	class FunctionLoweringContext;

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
