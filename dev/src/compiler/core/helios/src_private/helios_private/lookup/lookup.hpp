// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "lookup_result.hpp"

#include <helios/scope_id.hpp>
#include <helios/symbols/symbol_id.hpp>

#include <query_framework/context/context_fd.hpp>
#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>
#include <string_id/string_id.hpp>

#include <variant>
#include <vector>

namespace compiler::helios {
	/**
	 * @brief What a `using`/`import` points at:
	 * * `ScopeID` -- the scope of the module or namespace named by a wildcard selector.
	 * * `std::vector<SymID>` -- the symbols named by a non-wildcard selector.
	 */
	using UsingImportTarget = std::variant<ScopeID, std::vector<SymID>>;

	/**
	 * @brief Resolves what the selector of a `using`/`import` symbol points at.
	 * It is independent of the looked-up name, nothing is looked up inside the target.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryResolveUsingImport, SymID, CRef<query::QResult<UsingImportTarget>>, ({}));

	/**
	 * @brief Can the `using`/`import` @p symbol provide @p name, judging by its selector alone.
	 * Wildcard selectors provide every name when @p with_wildcards is set.
	 */
	query::QResult<bool> usingImportProvidesName(
		query::Context& ctx, SymID symbol, base::StrID name, bool with_wildcards
	);

	/**
	 * @brief Scope holding the contents of the module or namespace @p symbol.
	 */
	ScopeID scopeOfNamespaceLike(query::Context& ctx, SymID symbol);
}
