#include "hout_walkers.hpp"

#include <helios/utils/get_expr_symid.hpp>
#include <helios/utils/hout_walker_generic.hpp>

#include <unordered_set>

namespace compiler::helios {
	std::vector<SymID> collectCalledSymbols(const HOUTFunction& fun) {
		std::vector<SymID> result;

		walkFunctionTree(fun, [&](const auto& node) {
			if constexpr (std::same_as<std::remove_cvref_t<decltype(node)>, code::CallExpr>) {
				auto callee_sym = getIdentifierExprSymID(node.callee.ref());
				if (callee_sym) result.push_back(*callee_sym);
			}
		});

		return result;
	}
}
