// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "lookup_chains.hpp"

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
	 * @brief A wildcard `using`/`import` of a module or namespace. The chain is what its selector
	 * names, like the `a.N` of `using a.N.*;`, and @p scope holds the contents of its leaf.
	 */
	struct TargetScope final {
		ScopeID      scope;
		LookupChains chain;
	};

	/**
	 * @brief A `using`/`import` without a wildcard. The chain is what its selector names, like the
	 * `a.b` of `using a.b;`, and its leaf is the provided symbol.
	 */
	struct TargetExpr final {
		LookupChains chain;
	};

	/**
	 * @brief A wildcard `using`/`import` of a symbol that is not namespace-like. The chain is what
	 * its selector names, like the `v` of `using v.*;`, and the wildcard looks into its leaf.
	 */
	struct WildcardExpr final {
		LookupChains chain;
	};

	/**
	 * @brief What a `using`/`import` points at:
	 * * `TargetScope` -- the module or namespace named by a wildcard selector.
	 * * `TargetExpr` -- the chain named by a non-wildcard selector.
	 * * `WildcardExpr` -- the chain of a symbol, that is not namespace-like, named by a wildcard
	 *   selector.
	 */
	using UsingImportTarget = std::variant<TargetScope, TargetExpr, WildcardExpr>;

	/**
	 * @brief Resolves what the selector of a `using`/`import` symbol points at.
	 * It is independent of the looked-up name, nothing is looked up inside the target. Every name
	 * of the selector has to be found uniquely.
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
