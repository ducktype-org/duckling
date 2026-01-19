#pragma once

#include "dvm_operation.hpp"
#include "dvm_value.hpp"

#include <lir/lir_structure/lir_structure.hpp>

namespace compiler::backend_vm::internal {

	class FunctionLoweringContext;

	/**
	 * @note This class exists only because it can be a friend of FunctionLoweringContext.
	 * Having only a function as a friend would require importing more headers to the .hpp file
	 * of FunctionLoweringContext.
	 */
	class CastOperationLowerer {
	public:
		/**
		 * @brief Generates instructions to perform a cast operation.
         * 
		 * @param cast_operation The cast operation to lower.
		 * @param args The arguments of the cast operation.
		 * @param maybe_output The optional output of the cast operation, but this functions panics
		 * if not present.
		 * @param function_context The function lowering context to use.I
		 */
		static void lowerCastOperation(
			const CastOperation&     cast_operation,
			std::deque<DVMValue>&    args,
			base::Optional<DVMValue> maybe_output,
			FunctionLoweringContext& program_context
		);
	};
}
