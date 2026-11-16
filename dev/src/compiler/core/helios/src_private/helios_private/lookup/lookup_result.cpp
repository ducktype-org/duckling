// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "lookup_result.hpp"

#include <helios/hout/elements/expr.hpp>
#include <helios/symbols/symbol_kind.hpp>

#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <diagnostic/placeholder.hpp>
#include <query_framework/context/context.hpp>

#include <algorithm>
#include <ranges>

namespace compiler::helios {
	namespace {
		bool isUsingOrImport(SymID symbol) {
			return kind(symbol) == SymbolKind::Using or kind(symbol) == SymbolKind::Import;
		}

		auto accessedSymbols(const LookupPath& path) {
			return path.symbols
			     | std::views::filter([](SymID symbol) { return not isUsingOrImport(symbol); });
		}

		query::QResult<Box<code::Expr>> pathAsExpression(
			query::Context& ctx, const LookupPath& path, code::ElementOrigin origin
		) {
			auto symbols = stripQualifiers(ctx, path, false).symbols;
			if (isInstanceMember(ctx, symbols.front())) {
				ctx.log<dia::NotYetImplementedCodeError>(
					base::strConcat("Expression of the looked-up symbol '", name(path.leaf()), "'"),
					origin.getStablePosition()
				);
				return query::Failed();
			}

			Box<code::Expr> expr = makeBox<code::IdentifierExpr>(ctx, origin, symbols.front());

			for (const SymID field: symbols | std::views::drop(1)) {
				if (kind(field) != SymbolKind::Field) {
					ctx.log<dia::NotYetImplementedCodeError>(
						base::strConcat("Expression of the looked-up symbol '", name(field), "'"),
						origin.getStablePosition()
					);
					return query::Failed();
				}

				if (expr->expression_type.getSymbolType().getRefKind() != tsh::ReferenceKind::Direct)
					expr = makeBox<code::DerefExpr>(ctx, origin.generatedFrom(), std::move(expr));
				expr = makeBox<code::AccessExpr>(ctx, origin, std::move(expr), field);
			}
			return expr;
		}
	}

	SymID LookupPath::leaf() const {
		CORE_ASSERT(not symbols.empty(), "Lookup path has no symbols");
		return symbols.back();
	}

	bool LookupPath::constructsAccessExpr() const {
		return std::ranges::distance(accessedSymbols(*this)) > 1;
	}

	bool LookupResult::isEmpty() const { return paths.empty(); }

	bool LookupResult::isSingle() const { return paths.size() == 1; }

	bool LookupResult::hasInaccessible() const { return not inaccessible.empty(); }

	std::vector<SymID> LookupResult::leaves() const {
		return paths | std::views::transform(&LookupPath::leaf) | std::ranges::to<std::vector>();
	}

	SymID LookupResult::uniqueLeaf() const {
		CORE_ASSERT(isSingle(), "Expected a single symbol in the lookup result");
		return paths.front().leaf();
	}

	bool LookupResult::constructsAccessExpr() const {
		return std::ranges::any_of(paths, &LookupPath::constructsAccessExpr);
	}

	query::QResult<std::vector<Box<code::Expr>>> LookupResult::asExpressions(
		query::Context& ctx, code::ElementOrigin origin
	) const {
		std::vector<Box<code::Expr>> expressions;
		for (const LookupPath& path: paths) {
			UNPACK_QRESULT_MOVE(auto expr =, pathAsExpression(ctx, path, origin));
			expressions.push_back(std::move(expr));
		}
		return expressions;
	}

	LookupPath stripQualifiers(query::Context& ctx, LookupPath path, bool keep_using_imports) {
		usize start = path.symbols.size();
		while (
			start > 0
			and (isUsingOrImport(path.symbols.at(start - 1)) or isInstanceMember(ctx, path.symbols.at(start - 1)))
		)
			start--;
		const usize first_kept = start == 0 ? 0 : start - 1;

		LookupPath result;
		for (usize i = 0; i < path.symbols.size(); i++) {
			const SymID symbol = path.symbols.at(i);
			if (isUsingOrImport(symbol)) {
				if (keep_using_imports) result.symbols.push_back(symbol);
			} else if (i >= first_kept) {
				result.symbols.push_back(symbol);
			}
		}
		return result;
	}
}
