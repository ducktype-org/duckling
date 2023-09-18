#pragma once

#include "symtable/symtable.hpp"
#include <queue>
#include <span>
#include <printer/printer.hpp>


namespace hir {

	using symtable::SymbolRef;
	using symtable::Symbol;

	typedef std::vector<symtable::SymbolRef> LookupResult; 

	class AnalysisState {
		std::queue<symtable::SymbolRef> to_analyze;
		symtable::SymbolData symbol_data;

		// @TODO: error state here

	public:

		bool empty() const { return to_analyze.empty(); }
		bool notEmpty() const { return !empty(); }
		
		SymbolRef popNext();

		void addSymbol(base::unique_ptr<Symbol> symbol);

		// @TODO: add const
		symtable::SymbolData& symTable() { return symbol_data; };

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
