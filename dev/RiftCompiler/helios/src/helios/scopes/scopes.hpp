#pragma once

#include <base/string_id.hpp>
#include <query_framework/query_int.hpp>
#include <base/perfect_hash.hpp>
#include <pst_parser/elements/elements.hpp>
#include <pst_parser/rift_parser_state.hpp>

#include "../pst_ref.hpp"
#include "../scope_symbol_id.hpp"
#include "../lookup_result.hpp"

// @TODO: relax this dependency
#include <frontend/module_tree/queries.hpp>

namespace compiler::helios {
	using StmtList = std::vector<PstRef<pst::Stmt>>;

	/**
	 * @brief Return parent scope or none for root-scopes.
	 */
	base::Optional<ScopeID> parent(ScopeID);

	/**
	 * @brief Return module the scope was defined in
	 */
	frontend::ModuleId module(ScopeID id);

	/**
	 * @brief Query root scope for given module.
	 */
	DECLARE_QUERY(QueryRootScopeOf, frontend::ModuleId, ScopeID);

	struct KeyOf_QueryPrimaryCodeScopeFor {
		/**
		 * @brief scope that queries scope will be contained within
		 * @fixme: it is highly bug prone, as one can create many scopes with arbitrary parent
		 * scopes for singular element, and it has already lead to crucial errors.
		 * On the other hand it is not trivial to eliminate it.
		 * Solution would be to either eliminate it or to add smart sanity checks, that
		 * can prevent at least some of potential bugs.
		 * For now a simple assertion is added to disallow "double parent" situation
		 */
		ScopeID parent;

		/**
		 * @brief Element for which the scope is created.
		 * @note: scopes of various elements behave differently
		 * For now scope of StatementAggravates and Functions are possible.
		 * Scope behaviour for:
		 * * StatementAggravates -- a scope of aggregated statements
		 * * Function -- a scope of function arguments (@todo: function scopes are currently empty)
		 */
		PstRef<pst::RiftElement> base_element;

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
