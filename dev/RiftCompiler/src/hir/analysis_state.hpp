#pragma once

#include "symtable/symtable.hpp"
#include <queue>
#include <span>

namespace hir {

	using symtable::Symbol;
	using symtable::SymbolRef;
	
	/**
	 * @brief Analysis state holds all information on
	 * the state of HIR-transformation. 
	 * That include: symbol data, scope data, queue of symbols to analyze.
	 */
	class AnalysisState {
		std::queue<symtable::SymbolRef> to_analyze;
		symtable::SymbolData            symbol_data;

		// @TODO: error state here

	public:
		bool empty() const { return to_analyze.empty(); }

		bool notEmpty() const { return !empty(); }

		SymbolRef popNext();

		void addSymbol(base::unique_ptr<Symbol> symbol);

		// @TODO: add const
		symtable::SymbolData& symTable() { return symbol_data; }

		template<typename T, typename... Args>
		void emplaceSymbol(Args&&... args) {
			addSymbol(base::make_unique<T>(args...));
		}

		auto newSubRootScope() { return symbol_data.newSubRootScope(); }

		auto newScope(symtable::ScopeRef parent, base::StrId name) {
			return symbol_data.newScope(parent, name);
		}

		void logError(std::string_view error) {
			// @TODO: temp
			RIFT_PANIC(error);
		}
	};
}
