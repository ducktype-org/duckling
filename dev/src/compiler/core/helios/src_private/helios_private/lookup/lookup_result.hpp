// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file lookup_result.hpp
 */

#pragma once

#include <helios/hout/origin.hpp>
#include <helios/symbols/symbol_id.hpp>

#include <base/pointers/box.hpp>

#include <query_framework/context/context_fd.hpp>
#include <query_framework/query_result.hpp>

#include <vector>

namespace compiler::helios::code {
	struct Expr;
}

namespace compiler::helios {
	/**
	 * @brief One symbol found by a lookup, together with the symbols it is accessed through.
	 *
	 * The leaf is the found symbol, but sometimes it requires the preceding symbols to construct
	 * proper "access" expression. The path consists of two independent type of informations:
	 * 1. The usings/imports we went through to find the symbol, which is used in error handling.
	 * 2. The values we need to construct the proper expression. For example `using a.x` where `a`
	 * is a value of some class, and `x` is a field will construct a path like `[a, x]`. There
	 * should be no modules or namespaces in the path, as they are not needed.
	 */
	struct LookupPath final {
		std::vector<SymID> symbols;

		/**
		 * @brief The found symbol.
		 */
		[[nodiscard]]
		SymID leaf() const;

		/**
		 * @brief Whether the path stands for an access expression on a value, i.e. there are other
		 * symbols than `using`/`import` above the leaf, like the `v` of `v.x`.
		 */
		[[nodiscard]]
		bool constructsAccessExpr() const;

		/**
		 * @brief The expression evaluating to the found symbol.
		 *
		 * The expression starts at the last symbol of the path that is not a non-static field or
		 * method, and the fields below it are accessed on it.
		 *
		 * @note Fails, with a logged error, for a path that does not contain such a symbol or has
		 * something else than fields below it.
		 */
		query::QResult<Box<code::Expr>> asExpression(
			query::Context& ctx, code::ElementOrigin origin
		) const;
	};

	/**
	 * @brief The symbols found by a lookup, every path being one found symbol.
	 */
	struct LookupResult final {
		std::vector<LookupPath> paths;

		/**
		 * @brief Symbols of the looked-up name that are hidden by their visibility from the scope
		 * the lookup was performed in. They are never a result of the lookup.
		 */
		std::vector<SymID> inaccessible;

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
		 * @brief Whether some path stands for an access expression (@see
		 * LookupPath::constructsAccessExpr).
		 */
		[[nodiscard]]
		bool constructsAccessExpr() const;

		/**
		 * @brief The expressions of all the paths (@see LookupPath::asExpression).
		 */
		query::QResult<std::vector<Box<code::Expr>>> asExpressions(
			query::Context& ctx, code::ElementOrigin origin
		) const;
	};

	/**
	 * @brief @p path without the symbols that only qualify its leaf, which are the ones above its
	 * last symbol that is not a non-static field or method, like the `a.N` of `a.N.v.x` or the `v`
	 * of `v.static_field`.
	 * @param keep_using_imports Whether the `using`/`import` symbols of @p path are kept.
	 */
	LookupPath stripQualifiers(query::Context& ctx, LookupPath path, bool keep_using_imports);
}
