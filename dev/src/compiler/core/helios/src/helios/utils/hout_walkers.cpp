// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "hout_walkers.hpp"

#include <helios/utils/get_expr_symid.hpp>
#include <helios/utils/hout_walker_generic.hpp>

#include <unordered_set>

namespace compiler::helios::code {
	namespace {
		/**
		 * @brief Builds a HOUT-tree handler that records, into `result`, the symbol of every
		 * function called by the visited nodes. Non-matching node types (statements, other
		 * expressions) are ignored.
		 */
		auto calledSymbolCollector(std::vector<SymID>& result, std::unordered_set<SymID>& visited) {
			return [&result, &visited](const auto& node) {
				auto add_to_result = [&](SymID element) {
					if (visited.insert(element).second) result.push_back(element);
				};
				using Node = std::remove_cvref_t<decltype(node)>;
				if constexpr (std::same_as<Node, CallExpr>) {
					if (const auto* callee_ident
					    = dynamic_cast<const IdentifierExpr*>(node.callee.get())) {
						add_to_result(callee_ident->symbol);
					}
				} else if constexpr (std::same_as<Node, TupleExpr>) {
					// A tuple expression lowers to a call to the tuple constructor in MIR.
					add_to_result(node.tuple_ctor_symbol);
				}
			};
		}
	}

	std::vector<SymID> collectCalledSymbolsFromHOUT(const HOUTFunction& fun) {
		std::vector<SymID>        result;
		std::unordered_set<SymID> visited;
		walkFunctionTree(fun, calledSymbolCollector(result, visited));
		return result;
	}

	std::vector<SymID> collectCalledSymbolsFromHOUT(const Expr& expr) {
		std::vector<SymID>        result;
		std::unordered_set<SymID> visited;
		walkExprTree(expr, calledSymbolCollector(result, visited));
		return result;
	}
}
