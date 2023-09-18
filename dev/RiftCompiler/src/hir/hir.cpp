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

		// @TODO: this should be sub_root_scope, but it is root_scope for now, so tests can work
		// Right now we can't lookup into sub root scopes
		// symtable::ScopeRef root_scope = analysis_state.newSubRootScope();
		symtable::ScopeRef root_scope = analysis_state.symTable().getRootScope();

		analysis_state.emplaceSymbol<TopLevelSymbol>(
			analysis_state,
			root_scope, base::StrId("TopLevel"),
			pst.getTopLevelElement()
		);

		root_scope->close();

		std::cerr << "Added top level symbol!\n";

		while (analysis_state.notEmpty()) {
			auto symbol_ref = analysis_state.popNext();

			std::cerr << "\n====================\n";
			std::cerr << "Analyzing next: " << symbol_ref->getName().strView() << "\n";

			symbol_ref->analyzeAll();

			std::cerr << "    Type: " << symbol_ref->getType().getType().show() << "\n";

			std::cerr << "    Value: ";
			if (symbol_ref->getKind() == SymbolKind::Const) {
				auto val = symbol_ref->getValue();
				if (val.getType().getType().getKind() == ts::Kind::Integral) {
					// @TODO: i32 here is temporary:
					std::cerr << val.getData<i32>().front() << "\n";
				}
				else {
					std::cerr << "<NOT INTEGRAL>\n";
				}
			}
			else {
				std::cerr << "<NOT CONST>\n";
			}

			
		}

	}


}