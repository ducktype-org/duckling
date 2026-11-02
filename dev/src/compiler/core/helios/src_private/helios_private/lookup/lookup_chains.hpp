// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file lookup_chains.hpp
 */

#pragma once

#include <helios/hout/origin.hpp>
#include <helios/symbols/symbol_id.hpp>

#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>

#include <query_framework/context/context_fd.hpp>
#include <query_framework/query_result.hpp>

#include <vector>

namespace compiler::helios::code {
	struct Expr;
}

namespace compiler::helios {
	struct LookupChains;

	/**
	 * @brief A symbol found by a lookup.
	 *
	 * A nested node is a symbol through which the @p inner results were found, like the `v` of
	 * `using v.field;` or the `N` of `using N.f;`.
	 */
	class Node final {
	public:
		SymID id;

		base::Optional<CRef<LookupChains>> inner;

		static Node symbol(SymID id) { return { id, {} }; }

		static Node nested(SymID id, CRef<LookupChains> inner) { return { id, inner }; }

		/**
		 * @brief The symbols found through this node, which are the leaves below it.
		 */
		[[nodiscard]]
		std::vector<SymID> leaves() const;

		/**
		 * @brief Whether some symbol found through this node is accessed through a value, which is
		 * the case when a symbol holding a value (@see isValueSymbol) is above its leaf.
		 */
		[[nodiscard]]
		bool passesThroughValue() const;

	private:
		Node(SymID id, base::Optional<CRef<LookupChains>> inner): id(id), inner(inner) {}
	};

	/**
	 * @brief Forest of the symbols found by a lookup.
	 *
	 * Every root-to-leaf path is one found symbol: the leaf is the symbol, and the nodes above it
	 * are the symbols (modules, namespaces, classes and values) it is accessed through.
	 */
	struct LookupChains final {
		std::vector<Node> nodes;

		/**
		 * @brief Symbols of the looked-up name that are hidden by their visibility from the scope
		 * the lookup was performed in. They are never a result of the lookup.
		 */
		std::vector<SymID> inaccessible;

		/**
		 * @brief Inner results created for the nodes of this result. Inner results not stored
		 * here are owned by the caches of other lookup queries.
		 */
		std::vector<Box<LookupChains>> storage;

		[[nodiscard]]
		bool isEmpty() const;

		[[nodiscard]]
		bool isSingle() const;

		[[nodiscard]]
		bool hasInaccessible() const;

		/**
		 * @brief The found symbols, which are the leaves of all the paths.
		 */
		[[nodiscard]]
		std::vector<SymID> leaves() const;

		/**
		 * @brief The only found symbol.
		 * @warning Panics if the result is not single.
		 */
		[[nodiscard]]
		SymID uniqueLeaf() const;

		/**
		 * @brief Whether some found symbol is accessed through a value (@see
		 * Node::passesThroughValue).
		 */
		[[nodiscard]]
		bool isAccessedThroughValue() const;

		/**
		 * @brief Turns every path into an expression evaluating to the found symbol.
		 *
		 * A path is built from its leaf up: the part of the path above the last symbol that stands
		 * for a value on its own (a variable, a parameter, a const or a static field) is cut off,
		 * and the fields below it are accessed on it.
		 *
		 * @note Fails, with a logged error, for paths that do not contain such a symbol.
		 */
		query::QResult<std::vector<Box<code::Expr>>> asExpressions(
			query::Context& ctx, code::ElementOrigin origin
		) const;
	};

	/**
	 * @brief Whether @p symbol holds a value: a variable, a parameter, a const or a field.
	 */
	bool isValueSymbol(SymID symbol);

	/**
	 * @brief Copy of the single path of @p unique, where the leaf becomes a nested node holding
	 * @p inner.
	 * @warning Panics if @p unique is not single.
	 */
	LookupChains withLeafInner(const LookupChains& unique, CRef<LookupChains> inner);

	/**
	 * @brief Appends to @p into, for every root of @p found, a copy of the single path of @p chain
	 * whose leaf holds that root. The inaccessible symbols of @p found are appended as they are.
	 * @note The storage of @p found is not taken over, the caller keeps it alive.
	 * @warning Panics if @p chain is not single.
	 */
	void appendUnderLeaf(LookupChains& into, const LookupChains& chain, const LookupChains& found);
}
