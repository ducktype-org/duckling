#pragma once

#include "lir_module_data.hpp"

#include <helios/hout/hout.hpp>

namespace compiler::driver {
	/**
	 * @brief Converts a HOUTUnit to LIRModuleData (lowers content to MIR and then LIR
	 * representation).
	 * @param ctx The query context.
	 * @param hout_unit The HOUTUnit to convert.
	 * @param module_id The "name" of the module, it will be later used by backends to give module
	 * its ID.
	 * @return A LIRModuleData object representing the HOUTUnit.
	 */
	LIRModuleData compileHOUTUnitToLIRModuleData(
		query::Context& ctx, base::CRef<compiler::helios::HOUTUnit> hout_unit, base::StrID module_id
	);
}
