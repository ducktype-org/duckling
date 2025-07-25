#pragma once

#include "backend_module_data.hpp"

#include <helios/hout/hout.hpp>

namespace compiler::driver {
	LIRModuleData compileHOUTUnitToLIRModuleData(
		query::Context& ctx, base::CRef<compiler::helios::HOUTUnit> hout_unit, base::StrID module_id
	);
}
