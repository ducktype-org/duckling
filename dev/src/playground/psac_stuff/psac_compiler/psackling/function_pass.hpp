#pragma once

#include "context.hpp"
#include "expr_lowering.hpp"
#include "llvm.hpp"
#include "llvm_lowering_context.hpp"

#include <frontend/pst_parser/elements/elements_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/actions/all_actions.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/all_class_elements.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/all_lists.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>

#include <deque>

namespace pg {
	/**
	 * @brief Second pass: lowers each function body into LLVM IR.
	 *
	 * Assumes GlobalPass has already populated Context with FunctionSymbols
	 * (and their lowered LLVM FunctionTypes via LlvmLoweringContext).
	 */
	class FunctionPass final: public pst::PstVisitorPanicky {
	public:
		FunctionPass(Context& ctx, LlvmProgramModule& llvm_module, LlvmLoweringContext& lowering):
			  ctx(ctx),
			  llvm_module(llvm_module),
			  lowering(lowering) {}

		void visitFun(pst::Access<pst::Fun> stmt) override;
		void visitIf(pst::Access<pst::If> stmt) override;
		void visitWhile(pst::Access<pst::While> stmt) override;
		void visitReturn(pst::Access<pst::Return> stmt) override;
		void visitVariable(pst::Access<pst::Variable> stmt) override;
		void visitExprStmt(pst::Access<pst::ExprStmt> stmt) override;
		void visitNamespace(pst::Access<pst::Namespace> stmt) override;
		void visitConst(pst::Access<pst::Const> stmt) override;

		// Classes are skipped in function pass.
		void visitClass(pst::Access<pst::Class>) override {}

	private:
		Context&             ctx;
		LlvmProgramModule&   llvm_module;
		LlvmLoweringContext& lowering;

		std::deque<llvm::IRBuilder<>> builder_stack;
		LocalsMap                     locals;

		/// Return type of the function currently being lowered; used to type-check returns.
		tp::TypeID current_return_type{};
	};
}
