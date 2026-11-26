// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file lookup_result.hpp
 *
 * @note Current lookup system assumes that every rhs of "." operator is
 * well looked-up single symbol.
 */

#pragma once

#include <helios/symbols/symbol_id.hpp>
#include <helios/utils/symbol_list.hpp>

#include <query_framework/query_result.hpp>

#include <cstdint>
#include <vector>

namespace compiler::helios {

	/**
	 * @brief Possible errors during lookupExpectUnique.
	 */
	namespace errors {
		class Ambiguity final {};

		class SymbolNotFound final {};

		/**
		 * @brief The name was found, but every symbol of that name is hidden by its visibility.
		 */
		class Inaccessible final {};
	}

	struct NestedResult;

	using GetAsSingleLookupQResult = query::QResult<
		std::variant<SymbolList, errors::Ambiguity, errors::SymbolNotFound, errors::Inaccessible>>;

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
		 * Symbols found under the looked-up name, that are not accessible from the scope the
		 * lookup was performed in.
		 *
		 * They are kept so that the error can say that the symbol exists but cannot be used,
		 * instead of saying that no such symbol exists. They are never treated as a result of the
		 * lookup, so an otherwise successful lookup ignores them.
		 */
		std::vector<SymID> inaccessible;

		/**
		 * Symbols through which results were found.
		 */
		std::vector<NestedResult> children;

		/**
		 * Nothing was found, but one of the searched modules failed to parse. The name may have
		 * been declared in the part the parser lost, and the parser has already reported why.
		 */
		bool searched_module_failed_to_parse = false;

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
		 * Check if the lookup has found any symbol that it had to hide because of its visibility.
		 * @return True if it has found one, false otherwise.
		 */
		[[nodiscard]]
		bool hasInaccessible() const;

		/**
		 * @brief Whether nothing was found, but the name may have been lost to a parse error.
		 * Such a result must not be reported as a missing name.
		 */
		[[nodiscard]]
		bool isMissingBecauseOfParseErrors() const;

		/**
		 * Returns a path to the symbol.
		 * Panics on ambiguity.
		 * Fails without a diagnostic when isMissingBecauseOfParseErrors().
		 * @return A SymbolList representing a path to the symbol.
		 */
		[[nodiscard]]
		GetAsSingleLookupQResult getAsSingle() const;

		/**
		 * Adds another LookupResult to self (leaves to leaves, children ot children).
		 */
		void merge(LookupResult other);

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
