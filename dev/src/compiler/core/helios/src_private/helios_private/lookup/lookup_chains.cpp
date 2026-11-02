// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "lookup_chains.hpp"

#include <helios/hout/elements/expr.hpp>
#include <helios/symbols/symbol_kind.hpp>

#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <diagnostic/placeholder.hpp>
#include <query_framework/context/context.hpp>

#include <algorithm>
#include <iterator>
#include <ranges>

namespace compiler::helios {
	namespace {
		using LookupPath = std::vector<SymID>;

		void collectPaths(
			const LookupChains& result, LookupPath& prefix, std::vector<LookupPath>& paths
		) {
			for (const Node& node: result.nodes) {
				prefix.push_back(node.id);
				if (node.inner.has_value())
					collectPaths(*node.inner.value(), prefix, paths);
				else
					paths.push_back(prefix);
				prefix.pop_back();
			}
		}

		std::vector<LookupPath> allPaths(const LookupChains& result) {
			LookupPath              prefix;
			std::vector<LookupPath> paths;
			collectPaths(result, prefix, paths);
			return paths;
		}

		bool standsAlone(query::Context& ctx, SymID symbol) {
			switch (kind(symbol)) {
			case SymbolKind::Variable:
			case SymbolKind::Parameter:
			case SymbolKind::Const:
				return true;
			case SymbolKind::Field:
				return isStaticField(ctx, symbol);
			default:
				return false;
			}
		}

		query::QResult<Box<code::Expr>> pathAsExpression(
			query::Context& ctx, const LookupPath& path, code::ElementOrigin origin
		) {
			usize start_index = path.size();
			while (start_index > 0 and not standsAlone(ctx, path.at(start_index - 1)))
				start_index--;
			if (start_index == 0) {
				ctx.log<dia::NotYetImplementedCodeError>(
					base::strConcat("Expression of the looked-up symbol '", name(path.back()), "'"),
					origin.getStablePosition()
				);
				return query::Failed();
			}
			start_index--;

			Box<code::Expr> expr = makeBox<code::IdentifierExpr>(ctx, origin, path.at(start_index));

			for (const SymID field: path | std::views::drop(start_index + 1)) {
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

	bool LookupChains::isEmpty() const { return nodes.empty(); }

	bool LookupChains::isSingle() const { return leaves().size() == 1; }

	bool LookupChains::hasInaccessible() const { return not inaccessible.empty(); }

	std::vector<SymID> LookupChains::leaves() const {
		return allPaths(*this)
		     | std::views::transform([](const LookupPath& path) { return path.back(); })
		     | std::ranges::to<std::vector>();
	}

	SymID LookupChains::uniqueLeaf() const {
		auto found = leaves();
		CORE_ASSERT(found.size() == 1, "Expected a single symbol in the lookup result");
		return found.front();
	}

	bool LookupChains::isAccessedThroughValue() const {
		return std::ranges::any_of(nodes, &Node::passesThroughValue);
	}

	query::QResult<std::vector<Box<code::Expr>>> LookupChains::asExpressions(
		query::Context& ctx, code::ElementOrigin origin
	) const {
		std::vector<Box<code::Expr>> expressions;
		for (const LookupPath& path: allPaths(*this)) {
			UNPACK_QRESULT_MOVE(auto expr =, pathAsExpression(ctx, path, origin));
			expressions.push_back(std::move(expr));
		}
		return expressions;
	}

	std::vector<SymID> Node::leaves() const {
		if (not inner.has_value()) return { id };
		return inner.value()->leaves();
	}

	bool Node::passesThroughValue() const {
		if (not inner.has_value()) return false;
		return isValueSymbol(id)
		    or std::ranges::any_of(inner.value()->nodes, &Node::passesThroughValue);
	}

	bool isValueSymbol(SymID symbol) {
		switch (kind(symbol)) {
		case SymbolKind::Variable:
		case SymbolKind::Parameter:
		case SymbolKind::Const:
		case SymbolKind::Field:
			return true;
		default:
			return false;
		}
	}

	LookupChains withLeafInner(const LookupChains& unique, CRef<LookupChains> inner) {
		CORE_ASSERT(unique.isSingle(), "withLeafInner called on a lookup result that is not single");

		const Node&  node = unique.nodes.front();
		LookupChains result;
		if (not node.inner.has_value()) {
			result.nodes.push_back(Node::nested(node.id, inner));
			return result;
		}

		auto rebuilt = makeBox<LookupChains>(withLeafInner(*node.inner.value(), inner));
		result.nodes.push_back(Node::nested(node.id, &*rebuilt));
		result.storage.push_back(std::move(rebuilt));
		return result;
	}

	void appendUnderLeaf(LookupChains& into, const LookupChains& chain, const LookupChains& found) {
		for (const Node& node: found.nodes) {
			auto root = makeBox<LookupChains>();
			root->nodes.push_back(node);
			auto attached = withLeafInner(chain, &*root);
			into.storage.push_back(std::move(root));
			into.nodes.insert(into.nodes.end(), attached.nodes.begin(), attached.nodes.end());
			std::ranges::move(attached.storage, std::back_inserter(into.storage));
		}
		into.inaccessible.insert(
			into.inaccessible.end(), found.inaccessible.begin(), found.inaccessible.end()
		);
	}
}
