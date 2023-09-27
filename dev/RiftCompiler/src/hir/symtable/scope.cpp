#include "scope.hpp"
#include "../analysis_state.hpp"
#include "symbol.hpp"

#include <base/defer.hpp>
#include <base/exceptions.hpp>
#include <iostream>
#include <span>

namespace symtable {

	LookupResult Scope::lookup(hir::AnalysisState &state, base::StrId name) {
		// go over local symbols
		// go over local aliases (are aliases symbols? - yes)
		// go over links -- wildcard alias -- static links can cutoff, dep links needs un-aliasing
		// using a.b.*; is realized by:
		//  wildcard_alias _ = a.b;
		//  un aliasing then can perform proper un-aliasing

		std::cerr << "     simple lookup of " << name.strView() << "\n";

		LookupResult result{ {}, {} };

		if (lookup_engaged) return result;
		lookup_engaged = true;
		defer(lookup_engaged = false);

		for (auto &symbol : getSymbols()) {
			std::cerr << "        i see: " << symbol->getName().strView() << "\n";
			if (symbol->isWildcard()) {
				auto wild_result = symbol->lookupIn(state, name);
				std::cerr << "        wild see res:";
				wild_result.dprint(std::cerr);
				std::cerr << "\n";
				if (!wild_result.isEmpty()) {
					std::cerr << "         adding child!\n";
					result.children.push_back(std::move(wild_result).toNode(symbol));
				}
			} else {
				if (symbol->getName() == name) result.leaves.push_back(symbol);
			}
		}
		return result;
	}

	LookupResult Scope::lookupMeAndParents(hir::AnalysisState &state, base::StrId name) {
		auto result = lookup(state, name);
		if (parent != nullptr) {
			// Reverse insertion order allow for linear result concatenation instead of quadratic
			auto parent_result = parent->lookupMeAndParents(state, name);
			parent_result.insert(std::move(result));
			return parent_result;
		} else {
			return result;
		}
	}
}
