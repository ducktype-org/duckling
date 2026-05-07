#pragma once

#include "context.hpp"
#include "expr_lowering.hpp"
#include "llvm.hpp"
#include "llvm_lowering_context.hpp"

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
		void visitReturn(pst::Access<pst::Return> stmt) override;
		void visitVariable(pst::Access<pst::Variable> stmt) override;

	private:
		Context&             ctx;
		LlvmProgramModule&   llvm_module;
		LlvmLoweringContext& lowering;

		std::deque<llvm::IRBuilder<>> builder_stack;
		LocalsMap                     locals;
	};
}
