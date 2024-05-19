#pragma once

#include <base/string_id.hpp>
#include <query_framework/query_int.hpp>
#include <base/perfect_hash.hpp>
#include <pst_parser/elements/elements.hpp>
#include <pst_parser/rift_parser_base.hpp>

#include "../pst_ref.hpp"
#include "../scope_symbol_id.hpp"
#include "../lookup_result.hpp"

// @TODO: relax this dependency
#include <frontend/module_tree/queries.hpp>

namespace compiler::helios {
	using StmtList = std::vector<PstRef<pst::Stmt>>;

	/**
	 * @brief Query root scope for given module.
	 */
	DECLARE_QUERY(QueryRootScopeOf, frontend::ModuleId, ScopeID);

	struct KeyOf_QueryPrimaryCodeScopeFor {
		// @TODO is this needed?
		ScopeID parent;

		// Stmt here makes no sense with getChildStmtsOf
		// The overall idea is fine, but needs some polishing
		PstRef<pst::RiftElement> base_element;
		// StmtList stmts;

		[[nodiscard]]
		base::HashT customPerfectHash() const;
		bool        operator==(const KeyOf_QueryPrimaryCodeScopeFor&) const = default;
	};

	/**
	 * @brief Query Scope for given PST element that will be the child of scope from the key.
	 *
	 * @note: this is not perfect and might be changed in the future.
	 * there is currently no association between parent and base_element
	 * values inside the key.
	 */
	DECLARE_QUERY(QueryPrimaryCodeScopeFor, KeyOf_QueryPrimaryCodeScopeFor, ScopeID);

	struct KeyOf_LookupInScope {
		ScopeID     scope;
		base::StrId name;
		bool        with_wildcards;

		[[nodiscard]]
		base::HashT customPerfectHash() const;
		bool        operator==(const KeyOf_LookupInScope&) const = default;
	};

	/**
	 * @brief Performs lookup of single name inside given scope.
	 */
	DECLARE_QUERY(QueryLookupInScope, KeyOf_LookupInScope, const LookupResult&);

	/**
	 * @brief Performs lookup of single name inside given scope and its parents.
	 */
	DECLARE_QUERY(QueryLookupInScopeAndParents, KeyOf_LookupInScope, const LookupResult&);

	/**
	 * @brief Query all symbols that are directly inside given scope.
	 */
	DECLARE_QUERY(QuerySymbolsInScope, ScopeID, const std::vector<SymID>&);
}
