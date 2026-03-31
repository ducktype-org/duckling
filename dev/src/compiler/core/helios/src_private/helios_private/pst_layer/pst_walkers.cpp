#include "pst_walkers.hpp"

#include <frontend/pst_parser/elements/hierarchy/class_elements/class_specifier_block.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/top_level.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/class_block.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>

#include <base/except/exceptions.hpp>

#include <query_framework/query_errors.hpp>

namespace compiler::helios {

	// @TODO: in the future: make some base for all elements that can be used here

	namespace internal {
		void visitClassStmts(
			query::Context&                   ctx,
			StmtList<pst::ClassStmt>&         out,
			pst::AccessLocked<pst::ClassStmt> stmt
		) {
			if (auto access_block_opt = stmt.unlock(ctx).dynamicCast<pst::ClassSpecifierBlock>()) {
				auto access_block = access_block_opt.value();
				for (auto e: *access_block->getBlock().unlock(ctx)) visitClassStmts(ctx, out, e);
			} else
				out.push_back(stmt);
		}

		/**
		 * @brief Returns all children statements of given ClassBlock
		 * Flattens access specifier blocks as their information is included in statements.
		 *
		 * @return StmtList
		 */
		StmtList<pst::ClassStmt> getChildStmtsOfClassBlock(
			query::Context& ctx, pst::AccessLocked<pst::LangElement> elem
		) {
			if (auto class_block = elem.unlock(ctx).dynamicCast<pst::ClassBlock>()) {
				StmtList<pst::ClassStmt> out;
				for (auto&& e: *class_block.value()) internal::visitClassStmts(ctx, out, e);
				return out;
			} else {
				const auto& element = *elem.unlock(ctx);
				CORE_PANIC(base::strConcat(
					"Bad Duckling Element in `getChildStmtsOfClassBlock`: ", typeid(element).name()
				));
			}
		}
	}

	StmtList<> getStmtsFromStmtAggregate(
		query::Context& ctx, pst::AccessLocked<pst::LangElement> locked
	) {
		auto elem_optional = locked.unlockOpt(ctx);
		if (!elem_optional.has_value()) {
			// @TODO: #1753 this throw may be suboptimal
			query::throwFailed();
		}

		auto elem = elem_optional.value();

		// @TODO: dont use dynamic_cast's here, but a visitor
		if (auto code_block = elem.dynamicCast<pst::CodeBlock>()) {
			StmtList<> out;
			for (auto&& e: *code_block.value()) out.emplace_back(e);
			return out;
		}
		if (auto code_block_or_stmt = elem.dynamicCast<pst::CodeBlockOrStmt>()) {
			StmtList<> out;
			auto       code_block_or_stmt_val = code_block_or_stmt.value();
			if (code_block_or_stmt_val->getType() == pst::CodeBlockOrStmt::Type::CodeBlock)
				for (auto&& e: *code_block_or_stmt_val->getCodeBlock().unlock(ctx))
					out.emplace_back(e);
			else
				out.emplace_back(code_block_or_stmt_val->getStmt());
			return out;
		}
		if (auto top_level = elem.dynamicCast<pst::TopLevel>()) {
			StmtList<> out;
			for (auto e: top_level.value()->getStatements()) out.emplace_back(e);
			return out;
		}
		if (elem.dynamicCast<pst::ClassBlock>()) {
			auto       elements = internal::getChildStmtsOfClassBlock(ctx, elem);
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
