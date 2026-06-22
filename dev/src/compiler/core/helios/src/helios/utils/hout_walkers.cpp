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
		auto calledSymbolCollector(std::unordered_set<SymID>& result) {
			return [&result](const auto& node) {
				using Node = std::remove_cvref_t<decltype(node)>;
				if constexpr (std::same_as<Node, CallExpr>) {
					if (const auto* callee_ident
					    = dynamic_cast<const IdentifierExpr*>(node.callee.get())) {
						result.insert(callee_ident->symbol);
					}
				} else if constexpr (std::same_as<Node, TupleExpr>) {
					// A tuple expression lowers to a call to the tuple constructor in MIR.
					result.insert(node.tuple_ctor_symbol);
				}
			};
		}
	}

	std::vector<SymID> collectCalledSymbols(const HOUTFunction& fun) {
		std::unordered_set<SymID> result;
		walkFunctionTree(fun, calledSymbolCollector(result));
		return result | std::ranges::to<std::vector>();
	}

	std::vector<SymID> collectCalledSymbols(const Expr& expr) {
		std::unordered_set<SymID> result;
		walkExprTree(expr, calledSymbolCollector(result));
		return result | std::ranges::to<std::vector>();
	}
}
