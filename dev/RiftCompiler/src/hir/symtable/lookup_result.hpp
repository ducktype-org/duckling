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

	/**
	 * @brief A list of symbols.
	 */
	typedef std::vector<SymbolRef> SymbolChain;

	/**
	 * @brief Debug-Prints given SymbolChain to  an ostream.
	 */
	void dprintSymbolChain(const SymbolChain&, std::ostream&);

	/**
	 * @brief Calculate a de-aliased SymbolChain, that is a symbol chain where alias-symbols are
	 * replaced with their de-aliased counterparts.
	 */
	SymbolChain deAliasSymbolChain(const SymbolChain&);

	struct LookupNode;

	/**
	 * @brief Tree like structure storing lookup result.
	 * Actual results are always stored in "leaves", while
	 * children are responsible for storing results hidden under some aliases.
	 *
	 * Intuitively LookupResult is a result of a single "." operator.
	 */
	struct LookupResult {
		std::vector<SymbolRef>  leaves;
		std::vector<LookupNode> children;

		bool isEmpty() const;

		bool        isSingle() const;
		SymbolChain getAsSingle();
		SymbolChain getAsSingleReverse();

		void       insert(LookupResult&& other);
		LookupNode toNode(SymbolRef node) &;
		LookupNode toNode(SymbolRef node) &&;

		void dprint(std::ostream&);
	};

	/**
	 * @brief LookupNode is used as simple pair-like struct to
	 * implement tree-like structure of LookupResult.
	 */
	struct LookupNode {
		SymbolRef    node;  // node should always be alias-like of using-like thing
		LookupResult inner;

		void dprint(std::ostream&);
	};

	/**
	 * @brief ChainLookupResult stores standard LookupResult with a prefix.
	 *
	 * Intuitively ChainLookupResult is a result of a single "a.b.c"-like expression where,
	 * all but the last symbol are uniquely defined.
	 */
	struct ChainLookupResult {
		SymbolChain  prefix;
		LookupResult result;

		bool isEmpty() const;

		bool        isSingle() const;
		SymbolChain getAsSingle();

		void dprint(std::ostream&);
	};
}
