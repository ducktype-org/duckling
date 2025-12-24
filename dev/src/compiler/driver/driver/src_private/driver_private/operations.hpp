#pragma once

#include "lir_module_data.hpp"

#include <frontend/module_tree/module_id.hpp>
#include <helios/hout/hout_fd.hpp>

namespace compiler::driver {
	/**
	 * @brief Query that produces LIRModuleData for given Duckling module.
	 */
	DECLARE_QUERY(CompileToLIRModuleData, frontend::ModuleID, query::QResult<LIRModuleData>, ({}))
}
