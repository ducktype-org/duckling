#pragma once
#include "symbol_ref.hpp"
#include <variant>

namespace hir {
	class AnalysisState;
}

namespace symtable {
	// @TODO: current lookup types allow
	// only for „last symbol in chain” to be overloaded. 
	// This makes sense for functions, but not for namespaces
	// Possible solution that will not modify this sections may
	// be unifying all „same” namespace declarations into single symbol

	typedef std::vector<SymbolRef> SymbolChain;

	void dprintSymbolChain(const SymbolChain&, std::ostream&);

	SymbolChain deAliasSymbolChain(hir::AnalysisState&, const SymbolChain&);

	struct LookupNode;

	struct LookupResult {
		std::vector<SymbolRef> leaves;
		std::vector<LookupNode> children;

		bool isEmpty() const;

		bool isSingle() const;
		SymbolChain getAsSingle();
		SymbolChain getAsSingleReverse();

		void insert(LookupResult&& other);
		LookupNode toNode(SymbolRef node) &;
		LookupNode toNode(SymbolRef node) &&;

		void dprint(std::ostream&);
	};

	struct LookupNode {
		SymbolRef node; // node should always be alias-like of using-like thing
		LookupResult inner;

		void dprint(std::ostream&);
	};

	struct ChainLookupResult {
		SymbolChain prefix;
		LookupResult result;

		bool isEmpty() const;
		
		bool isSingle() const;
		SymbolChain getAsSingle();

		void dprint(std::ostream&);
	};
}


