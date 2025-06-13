#pragma once

#include "../lir_structure/lir_structure.hpp"

#include <helios/hout/hout.hpp>
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

	lir::LirGlobal lowerGlobalDataToLirGlobal(
		const helios::HOUTGlobalData& global_data, query::Context& ctx
	);
}
