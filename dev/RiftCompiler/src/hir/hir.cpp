#include <pst_parser/elements/elements.hpp>
#include <base/exceptions.hpp>
#include <base/unique_pointer.hpp>

#include "hir.hpp"
#include "analysis_state.hpp"

#include <queue>

#include <iostream>

namespace hir {

	void HIR::addUnit(SourceUnit&& unit) {
		if (unit.pst.getErrorState().fail()) {
			unit.pst.getErrorState().dumpLog(std::cerr);
		}
		sources.emplace_back(std::move(unit));
	}

	void HIR::doMagicStuff() {
		// Fow now we make just single PST

		if (sources.size() != 1) {
			throw base::NotYetImplemented("Source count not equal to 1");
		}

		// here we assume import where already done
		// analyze symbols one by one bfs like
		// if symbol tries to use other then we analyze that symbol to the point we need
		// @TODO
		// perform all stuff like @compile_if(1 > 2) -- this requires exec
		// expand macros -- macro can use symbol below
		// macro can't delete symbol
		// perhaps go top to to bottom with use/usings/expands 

		pst::PST& pst = sources[0].pst;

		AnalysisState state;

		symtable::ScopeRef root_scope = state.newSubRootScope();

		state.emplaceSymbol<TopLevelSymbol>(
			root_scope, base::StrId("TopLevel"),
			pst.getTopLevelElement()
		);

		std::cerr << "Added top level symbol!\n";

		while (state.notEmpty()) {
			auto symbol_ref = state.popNext();

			std::cerr << "Analyzing next: " << symbol_ref->getName().strView() << "\n";

			symbol_ref->analyzeAll(state);

			std::cerr << "    Type: " << symbol_ref->getType().getType().show() << "\n";
		}

	}


}