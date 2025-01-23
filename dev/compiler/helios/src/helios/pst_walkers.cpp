#include "pst_walkers.hpp"

#include <base/exceptions.hpp>
#include <pst_parser/elements/elements.hpp>

namespace compiler::helios {

	// @TODO: in the future: make some base for all elements that can be used here

	namespace detail {
		void visitClassStmts(StmtList<pst::ClassStmt>& out, MCRef<pst::ClassStmt> stmt) {
			if (auto* ptr = dynamic_cast<const pst::AccessBlock*>(&*stmt))
				for (auto&& e: *ptr->getBlock()) visitClassStmts(out, e);
			else
				out.push_back(stmt);
		}

		/**
		 * @brief Returns all children statements of given ClassBlock
		 * Flattens access specifier blocks as their information is included in statements.
		 *
		 * @return StmtList
		 */
		StmtList<pst::ClassStmt> getChildStmtsOfClassBlock(MCRef<pst::LangElement> elem) {
			if (auto* ptr = dynamic_cast<const pst::ClassBlock*>(&*elem)) {
				StmtList<pst::ClassStmt> out;
				for (auto&& e: *ptr) detail::visitClassStmts(out, e);
				return out;
			} else {
				const auto& element = *elem;
				CORE_PANIC(base::strConcat(
					"Bad Duckling Element in `getChildStmtsOfClassBlock`: ", typeid(element).name()
				));
			}
		}
	}

	StmtList<> getStmtsFromStmtAggregate(MCRef<pst::LangElement> elem) {
		// @TODO: dont use dynamic_cast's here, but a visitor
		if (auto* ptr = dynamic_cast<const pst::CodeBlock*>(&*elem)) {
			StmtList<> out;
			for (auto&& e: *ptr) out.emplace_back(e);
			return out;
		}
		if (auto* ptr = dynamic_cast<const pst::CodeBlockOrStmt*>(&*elem)) {
			StmtList<> out;
			for (auto&& e: *ptr) out.emplace_back(e);
			return out;
		}
		if (auto* ptr = dynamic_cast<const pst::TopLevel*>(&*elem)) {
			StmtList<> out;
			for (auto& e: ptr->getStatements()) out.emplace_back(e.ref());
			return out;
		}
		if (dynamic_cast<const pst::ClassBlock*>(&*elem)) {
			auto       elements = detail::getChildStmtsOfClassBlock(elem);
			StmtList<> out;
			for (auto e: elements) out.emplace_back(e);
			return out;
		}

		const auto& element = *elem;
		CORE_PANIC(base::strConcat(
			"Bad Duckling Element in `getStmtsFromStmtAggregate`: ", typeid(element).name()
		));
	}
}
