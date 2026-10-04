

#pragma once

#include "../lir_structure/lir_structure.hpp"

#include <mir/mir_structure/mir_structure.hpp>

namespace compiler::lir {

	/**
	 * @brief Lowers a MIR unit to a LIR unit.
	 * This is the main entry point and the source of truth for the MIR to LIR lowering logic.
	 */
	LIRUnit lowerToLIRUnit(query::Context& ctx, const mir::MIRUnit& mir_unit);
}
