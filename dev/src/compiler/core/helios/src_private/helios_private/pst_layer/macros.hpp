// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <frontend/pst_parser/generic_query_key.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {

	/**
	 * @brief Query the expansion of an expand statement.
	 *
	 * @note Ideally, this query should not be used outside of HELIOS PST-layer directory.
	 *
	 * \parallel owns its cache; creates PST via \ref pst::fromExpand (PST creation thread-safe)
	 */
	DECLARE_QUERY(
		QueryMacroExpansion,
		pst::GenericPSTQueryKey<pst::Expand>,
		query::QResult<pst::AccessLocked<pst::Stmt>>,
		({})
	)

}
