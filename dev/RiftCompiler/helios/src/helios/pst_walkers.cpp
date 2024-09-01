#include "pst_walkers.hpp"

#include <base/exceptions.hpp>
#include <pst_parser/elements/elements.hpp>

namespace compiler::helios {

	// @TODO: in the future: make some base for all elements that can be used here

	namespace detail {
		void visitClassStmts(StmtList<pst::ClassStmt>& out, PstRef<pst::ClassStmt> stmt) {
			if (auto* ptr = dynamic_cast<const pst::AccessBlock*>(stmt.get()))
				for (auto&& e: ptr->getBlock()) visitClassStmts(out, e);
			else
				out.push_back(stmt);
		}
	}

	StmtList<pst::ClassStmt> getChildStmtsOfClass(PstRef<pst::RiftElement> elem) {
		if (auto* ptr = dynamic_cast<const pst::ClassBlock*>(elem.get())) {
			StmtList<pst::ClassStmt> out;
			for (auto&& e: *ptr) detail::visitClassStmts(out, e);
			return out;
		} else {
			const auto& element = *elem.get();
			RIFT_PANIC(
				base::strConcat("Bad Rift Element in `getChildStmtsOfClass`: ", typeid(element).name())
			);
		}
	}

	StmtList<> getChildStmtsOf(PstRef<pst::RiftElement> elem) {
		if (auto* ptr = dynamic_cast<const pst::CodeBlock*>(elem.get())) {
			StmtList<> out;
			for (auto&& e: *ptr) out.emplace_back(e);
			return out;
		}
		if (auto* ptr = dynamic_cast<const pst::Class*>(elem.get())) {
			auto out = getChildStmtsOfClass(ptr->getBody());
			return {out.begin(), out.end()};
		}
		if (auto* ptr = dynamic_cast<const pst::CodeBlockOrStmt*>(elem.get())) {
			StmtList<> out;
			for (auto&& e: *ptr) out.emplace_back(e);
			return out;
		}
		if (auto* ptr = dynamic_cast<const pst::TopLevel*>(elem.get())) {
			StmtList<> out;
			for (auto&& e: ptr->getStatements()) out.emplace_back(e.borrow());
			return out;
		} else {
			const auto& element = *elem.get();
			RIFT_PANIC(
				base::strConcat("Bad Rift Element in `getChildStmtsOf`: ", typeid(element).name())
			);
		}
	}

}
