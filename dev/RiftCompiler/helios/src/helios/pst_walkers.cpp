#include "pst_walkers.hpp"

#include <base/exceptions.hpp>
#include <pst_parser/elements/elements.hpp>

namespace compiler::helios {

	// @TODO: in the future: make some base for all elements that can be used here

	StmtList getChildStmtsOf(PstRef<pst::RiftElement> elem) {
		if (auto* ptr = dynamic_cast<const pst::CodeBlock*>(elem.get())) {
			StmtList out;
			for (auto&& e: *ptr) out.emplace_back(e);
			return out;
		}
		if (auto* ptr = dynamic_cast<const pst::CodeBlockOrStmt*>(elem.get())) {
			StmtList out;
			for (auto&& e: *ptr) out.emplace_back(e);
			return out;
		}
		if (auto* ptr = dynamic_cast<const pst::TopLevel*>(elem.get())) {
			StmtList out;
			for (auto&& e: ptr->getStatements()) out.emplace_back(e.borrow());
			return out;
		} else {
			RIFT_PANIC("Bad Rift Element in `getChildStmtsOf`.");
		}
	}

}
