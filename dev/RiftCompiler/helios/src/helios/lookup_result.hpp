/**
 * @file lookup_result.hpp
 *
 * @note Current lookup system assumes that every rhs of "." operator is
 * well looked-up single symbol.
 */

#pragma once

#include "scope_symbol_id.hpp"

#include <vector>
#include <query_framework/query_int.hpp>

namespace compiler::helios {

	using SymbolList = std::vector<SymID>;

	struct NestedResult;

	/**
	 * @brief Tree like structure storing lookup result.
	 * Actual results are always stored in "leaves", while
	 * children are responsible for storing results hidden under some aliases.
	 *
	 * Intuitively LookupResult is a result of a single "." operator.
	 */
	struct LookupResult {
		std::vector<SymID>        leaves;
		std::vector<NestedResult> children;

		[[nodiscard]]
		bool isEmpty() const;

		[[nodiscard]]
		bool isSingle() const;
		[[nodiscard]]
		SymbolList getAsSingle() const;
		void       insert(LookupResult other);

		[[nodiscard]]
		NestedResult toNode(SymID node) const;

		[[nodiscard]]
		u64 symbolCount() const;
	};

	/**
	 * @brief NestedResult is used to represent result of lookup that was hidden
	 * behind some alias. "node" represent the alias, while "inner" represent
	 * lookup result behind the alias.
	 */
	struct NestedResult {
		SymID        node;  ///< node should always be alias-like of using-like thing
		LookupResult inner;
	};

	// @TODO: do we want ChainLookupResult for stuff like aliases, usings etc?

}
