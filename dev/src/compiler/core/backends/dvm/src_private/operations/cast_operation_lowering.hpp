#pragma once

#include "../dvm_operation.hpp"
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
		 * @brief Generates instructions to perform a cast operation and store the result in the @p
		 * output place.
		 *
		 * @param ctx The function lowering context to use.
		 * @param cast_operation The cast operation to lower.
		 * @param args The arguments of the cast operation.
		 * @param output The output place to store the result.
		 */
		static void lowerCastOperation(
			FunctionLoweringContext& ctx, const CastOperation& cast_operation
		);
	};
}
