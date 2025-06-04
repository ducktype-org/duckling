#include "symbol_list.hpp"

namespace compiler::helios {
	void SymbolList::appendList(const SymbolList& other) {
		list.insert(list.end(), other.list.begin(), other.list.end());
	}
}
