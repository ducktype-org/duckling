#include "scope.hpp"
#include "symbol.hpp"

#include <base/exceptions.hpp>
#include <base/defer.hpp>
#include <span>
#include <iostream>

namespace symtable {

	void Scope::addSymbol(const SymbolRef& symbol) {
		RIFT_ASSERT(state == ScopeState::Open, "Can not add symbols to closed scope");
		symbols.push_back(symbol);
	}

	LookupResult Scope::lookup(base::StrId pass_name) {
		// go over local symbols
		// go over local aliases (are aliases symbols? - yes)
		// go over links -- wildcard alias -- static links can cutoff, dep links needs un-aliasing
		// using a.b.*; is realized by:
		//  wildcard_alias _ = a.b;
		//  un aliasing then can perform proper un-aliasing

		std::cerr << "     Simple lookup of " << pass_name.strView();
		std::cerr << " in " << name.strView();
		std::cerr << "\n";

		RIFT_ASSERT(state == ScopeState::Closed, "Can not perform lookup in open scope");


		LookupResult result{ {}, {} };

		// @FIXME: this is probably a heuristic, and just a hotfix
		// In the future something better has to be done
		if (engaged_names.contains(pass_name)) return result;
		engaged_names.insert(pass_name);
		defer(engaged_names.erase(pass_name));

		std::cerr << "         lookup actually being done\n";

		for (auto& symbol: getSymbols()) {
			std::cerr << "        i see: " << symbol->getName().strView() << "\n";
			if (symbol->isWildcard() and symbol->unlockedLookup()) {
				std::cerr << "         looking in wildcard!\n";
				auto wild_result = symbol->lookupIn(pass_name);
				std::cerr << "        wild see res:";
				wild_result.dprint(std::cerr);
				std::cerr << "\n";
				if (!wild_result.isEmpty()) {
					std::cerr << "         adding child!\n";
					result.children.push_back(std::move(wild_result).toNode(symbol));
				}
			} else {
				if (symbol->getName() == pass_name) result.leaves.push_back(symbol);
			}
		}
		return result;
	}

	LookupResult Scope::lookupMeAndParents(base::StrId pass_name) {
		auto result = lookup(pass_name);
		if (parent != nullptr) {
			// Reverse insertion order allow for linear result concatenation instead of quadratic
			auto parent_result = parent->lookupMeAndParents(pass_name);
			parent_result.insert(std::move(result));
			return parent_result;
		} else {
			return result;
		}
	}

	void Scope::close() {
		RIFT_ASSERT(state == ScopeState::Open, "Can not close closed scope.");
		state = ScopeState::Closed;
	}
}
