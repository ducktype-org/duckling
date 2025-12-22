#pragma once

#include "lir_module_data.hpp"

#include <helios/hout/hout_fd.hpp>
#include <frontend/module_tree/module_id.hpp>

namespace compiler::driver {
	DECLARE_QUERY(
		CompileToLIRModuleData,
		frontend::ModuleID,
		query::QResult<LIRModuleData>,
		({})
	)
}
