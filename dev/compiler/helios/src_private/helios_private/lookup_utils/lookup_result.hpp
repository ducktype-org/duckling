/**
 * @file lookup_result.hpp
 *
 * @note Current lookup system assumes that every rhs of "." operator is
 * well looked-up single symbol.
 */

#pragma once

#include <helios/helios_errors.hpp>
#include <helios/helios_result.hpp>
#include <helios/scope_symbol_id.hpp>
#include <helios/utils/symbol_list.hpp>
#include <query_framework/context_fd.hpp>

#include <base/variant.hpp>

#include <vector>

namespace compiler::helios {

	errors::HResult<SymbolList, errors::Failed> dealiasSymbolList(
		query::Context& ctx, const SymbolList& symbol_list
	);

	struct NestedResult;

	/**
	 * @brief Tree like structure storing lookup result.
	 * Actual results are always stored in "leaves", while
	 * children are responsible for storing results hidden under some aliases.
	 *
	 * Intuitively LookupResult is a result of a single "." operator.
	 */
	struct LookupResult final {
		/**
		 * Direct symbols found.
		 */
		std::vector<SymID> leaves;
		/**
		 * Symbols through which results were found.
		 */
		std::vector<NestedResult> children;

		/**
		 * Check if lookup actually found something.
		 * @return True if it has found something, false otherwise.
		 */
		[[nodiscard]]
		bool isEmpty() const;

		/**
		 * Check if lookup has found exactly one symbol.
		 * @return True if exactly one, false otherwise.
		 */
		[[nodiscard]]
		bool isSingle() const;

		/**
		 * Returns a path to the symbol.
		 * Panics on ambiguity.
		 * @return A SymbolList representing a path to the symbol.
		 */
		[[nodiscard]]
		errors::HResult<SymbolList, errors::Ambiguity, errors::SymbolNotFound> getAsSingle() const;
		void insert(LookupResult other);

		/**
		 * Turns LookupResult into NestedResult referencing node.
		 * @param node SymID, that the NestedResult represents.
		 * @return New NestedResult from self with node.
		 */
		[[nodiscard]]
		NestedResult toNode(SymID node) const;

		/**
		 * Count the number of found symbols.
		 * @return The number of found symbols.
		 */
		[[nodiscard]]
		u64 symbolCount() const;
	};

	/**
	 * @brief NestedResult is used to represent result of lookup that was hidden
	 * behind some alias. "node" represent the alias, while "inner" represent
	 * lookup result behind the alias.
	 */
	struct NestedResult final {
		SymID        node;  ///< node should always be alias-like of using-like thing
		LookupResult inner;

		NestedResult(SymID node, LookupResult inner);
	};

	// @TODO: do we want ChainLookupResult for stuff like aliases, usings etc?

}
