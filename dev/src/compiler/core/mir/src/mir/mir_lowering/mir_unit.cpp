#pragma once

#include "../mir_structure/mir_structure.hpp"

#include <helios/hout/hout.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::mir {
	query::QResult<MIRUnit> lowerToMIRUnit(query::Context&, CRef<helios::HOUTUnit> hout_unit);
}
