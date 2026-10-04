#pragma once

#include "lir_unit_with_name.hpp"

#include <frontend/module_tree/module_id.hpp>
#include <helios/hout/hout_fd.hpp>

namespace compiler::driver {


	/**
	 * @brief Converts HOUTUnit to LIRUnitWithBackendName.
	 * @note It also prints and/or saves in artifacts IR representations if requested by options.
	 */
	query::QResult<LIRUnitWithBackendName> compileHOUTUnitToLIRModuleData(
		query::Context&         ctx,
		const helios::HOUTUnit& hout_unit,
		base::StrID             module_id,
		base::StrID             module_id_human
	);

	/**
	 * @brief Produces LIRUnitWithBackendName for given Duckling module.
	 */
	query::QResult<LIRUnitWithBackendName> compileModuleToLIRModuleData(
		query::Context& ctx, frontend::ModuleID module_id
	);
}
