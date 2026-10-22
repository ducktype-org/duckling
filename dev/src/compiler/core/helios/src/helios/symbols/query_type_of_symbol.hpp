// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once


#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {

	using QuerySymbolType_Result = query::QResult<tsh::SymbolType<>>;

	/**
	 * @brief Query type of the symbol.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryTypeOfSymbol, SymID, CRef<QuerySymbolType_Result>, ({}))

	/**
	 * @brief Return the destructor symbol for the type, if it is non-trivial.
	 * If it is trivial, return empty optional.
	 * @note This a public API the HELIOS code.
	 */
	base::Optional<helios::SymID> getTypeDestructor(query::Context&, tsh::SymbolType<>);
}
