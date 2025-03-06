#include "pst_walkers.hpp"

#include <base/exceptions.hpp>
#include <pst_parser/elements/elements.hpp>

namespace compiler::helios {

	// @TODO: in the future: make some base for all elements that can be used here

	namespace detail {
		void visitClassStmts(query::detail::ContextType& ctx, StmtList<pst::ClassStmt>& out, pst::AccessLocked<pst::ClassStmt> stmt) {
			if (auto accessBlock = stmt.unlock(ctx).dynamicCast<pst::AccessBlock>())
				for (auto e: *accessBlock->getBlock().unlock(ctx)) visitClassStmts(ctx, out, e);
			else
				out.push_back(stmt);
		}

		/**
		 * @brief Returns all children statements of given ClassBlock
		 * Flattens access specifier blocks as their information is included in statements.
		 *
		 * @return StmtList
		 */
		StmtList<pst::ClassStmt> getChildStmtsOfClassBlock(query::detail::ContextType& ctx, pst::AccessLocked<pst::LangElement> elem) {
			if (auto classBlock = elem.unlock(ctx).dynamicCast<pst::ClassBlock>()) {
				StmtList<pst::ClassStmt> out;
				for (auto&& e: *classBlock) detail::visitClassStmts(ctx, out, e);
				return out;
			} else {
				const auto& element = *elem.unlock(ctx);
				CORE_PANIC(base::strConcat(
					"Bad Duckling Element in `getChildStmtsOfClassBlock`: ", typeid(element).name()
				));
			}
		}
	}

	StmtList<> getStmtsFromStmtAggregate(query::detail::ContextType& ctx, pst::AccessLocked<pst::LangElement> locked) {
		auto elem = locked.unlock(ctx);
		// @TODO: dont use dynamic_cast's here, but a visitor
		if (auto codeBlock = elem.dynamicCast<pst::CodeBlock>()) {
			StmtList<> out;
			for (auto&& e: *codeBlock) out.emplace_back(e);
			return out;
		}
		if (auto codeBlockOrStmt = elem.dynamicCast<pst::CodeBlockOrStmt>()) {
			StmtList<> out;
			for (auto&& e: *codeBlockOrStmt) out.emplace_back(e);
			return out;
		}
		if (auto topLevel = elem.dynamicCast<pst::TopLevel>()) {
			StmtList<> out;
			for (auto e: topLevel->getStatements()) out.emplace_back(e);
			return out;
		}
		if (elem.dynamicCast<pst::ClassBlock>()) {
			auto       elements = detail::getChildStmtsOfClassBlock(ctx, elem);
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
