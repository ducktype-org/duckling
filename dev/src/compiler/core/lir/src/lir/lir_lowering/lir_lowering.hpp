#pragma once

#include "../lir_structure/lir_structure.hpp"

#include <mir/mir_structure/mir_structure.hpp>

namespace compiler::lir {
	struct KeyOf_LowerToLirFunction {
		CRef<mir::Function> function;

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const;
	};

	/**
	 * @brief Lower a MIRFunction to a LIRFunction
	 * Generates TSL types
	 */
	DECLARE_QUERY(LowerToLirFunction, KeyOf_LowerToLirFunction, CRef<Function>)

	/**
	 * @brief Creates a LIR function that call each function literal in the list
	 * This was useful to create one module ctor, that calls all ctors of globals
	 * In the provided order.
	 */
	Function fromFunctionLiterals(
		query::Context& ctx, const std::vector<FunctionLiteral>& function_literals
	);
}
