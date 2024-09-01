/** @file scopes.hpp
 *  @brief This file defines Queries responsible for creation of Scopes and operations on them.
 */
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
	 * @todo: Right now RootScopes are empty, and in order to access proper module
	 * symbols, one need to get scope of root element of the main module file.
	 * This should be somehow refactored when multi-file modules will be introduced.
	 * @todo: Currently root scopes are somewhat problematic.
	 * See description of "root_element_file_back_map" for details.
	 */
	DECLARE_QUERY(QueryRootScopeOf, frontend::ModuleId, ScopeID);

	struct KeyOf_QueryPrimaryCodeScopeFor {
		/**
		 * @brief Element for which the scope is created.
		 * @note: scopes of various elements behave differently
		 * Scope behaviour for:
		 * * StatementAggregates -- a scope of aggregated statements
		 * * Function -- a scope of function arguments (@todo: function scopes are currently empty)
		 * * Namespaces -- empty Scope
		 * * Classes -- scope containing class fields
		 * * Expr -- empty Scope
		 * * Return -- empty Scope
		 */
		PstRef<pst::RiftElement> base_element;

		[[nodiscard]]
		base::HashT customPerfectHash() const;
		bool        operator==(const KeyOf_QueryPrimaryCodeScopeFor&) const = default;
	};

	/**
	 * @brief Query Scope for given PST element.
	 * @note: Primary Scopes are linked directly to PST structure.
	 * This means that every PST element has a scope, even for some it doesn't make a lot of sense.
	 * The reason for this is that handling scope structure without direct link to PST was highly
	 * bug prone and led to potential errors or lack of consistency between different fragments of
	 * code.
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
	 *
	 * @NOTE: For structs, it returns what's inside struct's body.
	 */
	DECLARE_QUERY(QuerySymbolsInScope, ScopeID, const std::vector<SymID>&);

	/**
	 * @brief Root scope of main module file is currently the "effective" root scope.
	 * See: QueryRootScope for details
	 * @todo: this has to change in the future
	 *
	 * @param module
	 * @return ScopeID
	 */
	ScopeID extendQueryRootScopeOfMainModuleFile(query::Context&, frontend::ModuleId module);
}
