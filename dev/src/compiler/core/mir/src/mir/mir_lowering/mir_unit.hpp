// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
