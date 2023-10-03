#include "analysis_state.hpp"

#include <iostream>

namespace hir {
	SymbolRef AnalysisState::popNext() {
		RIFT_ASSERT(notEmpty(), "getNext called on empty AnalysisState");
		auto sym = to_analyze.front();
		to_analyze.pop();
		return sym;
	}

	void AnalysisState::addSymbol(base::unique_ptr<Symbol> symbol) {
		std::cerr << " ADD SYMBOL: `" << symbol->getName().strView() << "`"
				  << " in ???" /*<< symbol->getScope().asInt()*/ << "\n";

		auto sym_ref = symbol_data.newSymbol(std::move(symbol));
		to_analyze.push(sym_ref);
	}

}  // namespace hir
