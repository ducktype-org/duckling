#pragma once

#include "../lir_structure/lir_structure.hpp"

#include <mir/mir_structure/mir_structure.hpp>

namespace compiler::lir {
	struct KeyOf_LowerToLIRFunction {
		CRef<mir::Function> function;

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const;
	};

	/**
	 * @brief Lower a MIRFunction to a LIRFunction
	 * Generates TSL types
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		LowerToLIRFunction, KeyOf_LowerToLIRFunction, CRef<Function>, ({ .uses_qresult = false })
	)

	/**
	 * @brief Creates a LIR function that call each function in the list (in the provided order).
	 * This was useful to create one module ctor, that calls all ctors of globals
	 * In the provided order. Same for dtors.
	 */
	Function createFunctionInvoker(
		query::Context&                    ctx,
		const std::vector<CRef<Function>>& functions,
		const base::StrID&                 mangled_name
	);
}
