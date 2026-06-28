#include <helios/hout/hout.hpp>
#include <helios/symbols/symbol_id.hpp>

namespace compiler::helios::code {
	/**
	 * @brief Collect the symbols of every function called from `fun`.
	 *
	 * Walks the whole body and, for each `code::CallExpr` whose callee resolves to a
	 * plain identifier, records its `SymID`. Calls with a non-identifier callee
	 * (e.g. pointers to functions, lambdas) are skipped. The result is deduplicated.
	 */
	[[nodiscard]]
	std::vector<SymID> collectCalledSymbols(const HOUTFunction& fun);


	/**
	 * @brief Collect the symbols of every function called from the expression tree.
	 */
	[[nodiscard]]
	std::vector<SymID> collectCalledSymbols(const Expr& expr);
}
