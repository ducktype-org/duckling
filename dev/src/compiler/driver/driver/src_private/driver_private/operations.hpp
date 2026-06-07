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
		query::Context& ctx, const helios::HOUTUnit& hout_unit, base::StrID module_name
	);

	/**
	 * @brief Query that produces LIRUnitWithBackendName for given Duckling module.
	 * @TODO: #2246 Consider removing this query and moving logic from it elsewhere
	 * or changing it into function (it only adds a module name).
	 */
	DECLARE_QUERY(
		CompileToLIRModuleData,
		frontend::ModuleID,
		CRef<query::QResult<LIRUnitWithBackendName>>,
		({})
	)
}
