#include "stmts_from_aggregate.hpp"

#include <frontend/pst_parser/elements/hierarchy/class_elements/class_specifier_block.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/top_level.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/class_block.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expand.hpp>
#include <helios_private/pst_layer/macros.hpp>

#include <base/except/exceptions.hpp>

#include <query_framework/context/context.hpp>
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

		StmtList<> output;

		// @TODO: dont use dynamic_cast's here, but a visitor
		if (auto code_block = elem.dynamicCast<pst::CodeBlock>()) {
			for (auto&& e: *code_block.value()) output.emplace_back(e);
		} else if (auto code_block_or_stmt = elem.dynamicCast<pst::CodeBlockOrStmt>()) {
			auto code_block_or_stmt_val = code_block_or_stmt.value();
			if (code_block_or_stmt_val->getType() == pst::CodeBlockOrStmt::Type::CodeBlock)
				for (auto&& e: *code_block_or_stmt_val->getCodeBlock().unlock(ctx))
					output.emplace_back(e);
			else
				output.emplace_back(code_block_or_stmt_val->getStmt());
		} else if (auto top_level = elem.dynamicCast<pst::TopLevel>()) {
			for (auto e: top_level.value()->getStatements()) output.emplace_back(e);
		} else if (elem.dynamicCast<pst::ClassBlock>()) {
			auto elements = internal::getChildStmtsOfClassBlock(ctx, elem);
			for (auto e: elements) output.emplace_back(e);
		} else {
			const auto& element = *elem;
			CORE_PANIC(base::strConcat(
				"Bad Duckling Element in `getStmtsFromStmtAggregate`: ", typeid(element).name()
			));
		}

		// Now expand macros in the output.

		// @TODO: #2407 we do a lot of unlocks here, maybe we could return
		// the vector of unlocked elements from this function.

		StmtList<> output_after_macro_expansion;
		for (auto stmt: output) {
			auto current_stmt = stmt.unlock(ctx);

			// Handle macro expansions in a loop to support nested expansions.
			while (current_stmt->getElementKind() == pst::ElementKind::Expand) {
				
				auto expand_result = ctx.query<QueryMacroExpansion>(
											current_stmt.template dynamicCast<pst::Expand>().value()
				);
				// @TODO: #1753 this valueOrThrow may be suboptimal
				auto expanded_stmt = expand_result.valueOrThrow();
				current_stmt = expanded_stmt.unlock(ctx);
			}

			output_after_macro_expansion.emplace_back(current_stmt);
		}

		return output_after_macro_expansion;
	}
}
