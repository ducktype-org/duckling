

#include "helios/hout/hout.hpp"
#include "helios/symbols/symbol_id.hpp"

namespace compiler::helios::code {
	/**
	 * @brief Collect the symbols of every function called from `fun`.
	 *
	 * Walks the whole body and, for each `code::CallExpr` whose callee resolves to a
	 * plain identifier, records its `SymID`. Calls with a non-identifier callee
	 * (e.g. an expression-valued callee) are skipped. The result preserves
	 * first-encounter order and may contain duplicated.
	 */
	[[nodiscard]]
	std::vector<SymID> collectCalledSymbols(const HOUTFunction& fun);
}
