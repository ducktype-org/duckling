#pragma once

#include "../mir_structure/mir_structure.hpp"

#include <helios/hout/hout.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::mir {

	/**
	 * @brief Lowers a HOUT unit to a MIR unit.
	 * This is the main entry point and the source of truth for the HOUT to MIR lowering logic.
	 */
	query::QResult<MIRUnit> lowerToMIRUnit(query::Context&, CRef<helios::HOUTUnit> hout_unit);
}
