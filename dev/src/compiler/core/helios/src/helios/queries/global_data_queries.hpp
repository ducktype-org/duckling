// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file global_data_queries.hpp
 * @brief Queries related to global data HOUT materialization.
 */
#pragma once

#include <helios/hout/hout.hpp>
#include <helios/symbols/symbol_id.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {
	/**
	 * @brief Query HOUT representation of a global constant, variable or static field.
	 *
	 * Expects key to point to a symbol whose data lives in the program rather than in a value,
	 * that is a const, a global variable or a static field of a class. A static field is built
	 * the same way a global variable is, only its declaration comes from a different PST element.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryHOUTGlobalData, SymID, CRef<query::QResult<HOUTGlobalData>>, ({}));

	/**
	 * @brief Returns the expression that should be used by the MIR global variable
	 * constructor - it in-place initializes the global.
	 */
	Box<code::Expr> getGlobalConstructorExpr(query::Context& ctx, CRef<HOUTGlobalData> global_data);

	/**
	 * @brief Returns the expression that should be used by the MIR global variable
	 * destructor - it takes the reference to the global and calls destructor function.
	 */
	base::Optional<Box<code::Expr>> getGlobalDestructorExpr(
		query::Context& ctx, CRef<HOUTGlobalData> global_data
	);
}
