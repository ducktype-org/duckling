// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <helios/hout/hout.hpp>
#include <helios/tsh/types.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios::defgen {
	SymID lengthMethodForType(query::Context& ctx, tsh::AbstractType type);

	/**
	 * @brief Get the compiler-generated HOUT representation of the length method for a type.
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryLengthMethod, tsh::AbstractType, CRef<query::QResult<HOUTFunction>>, ({}));
}
