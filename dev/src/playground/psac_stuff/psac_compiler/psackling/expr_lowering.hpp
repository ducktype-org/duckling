#pragma once

#include "context.hpp"
#include "ids.hpp"
#include "llvm.hpp"
#include "llvm_lowering_context.hpp"

#include <frontend/pst_parser/elements/hierarchy/not_statements/expr_element.hpp>
#include <frontend/pst_parser/pst_expr_visitor.hpp>

#include <base/collections/maps.hpp>

namespace pg {
	struct LoweredExpr {
		llvm::Value* value{ nullptr };    // In r-value sense.
		tp::TypeID   type_id{};
		llvm::Value* address{ nullptr };  // In l-value sense.

		/// Set when the expression names a function or namespace (no runtime value yet);
		/// consumed by call and `::` access links in chain expressions.
		base::Optional<sym::SymbolID> symbol{};
	};

	/// Per-function map: variable SymbolID -> its stack slot (alloca).
	using LocalsMap = base::HashMap<sym::SymbolID, llvm::AllocaInst*>;

	/**
	 * @brief Lowers a single PST expression into LLVM IR.
	 *
	 * Short-lived helper constructed by FunctionPass for each expression site.
	 * All collaborators are held by reference; the ExprLowering itself owns no state
	 * besides the result of the most recent lower() call.
	 */
	class ExprLowering final: public pst::expr::PstExprVisitorPanicky {
	public:
		ExprLowering(
			Context& ctx, LlvmLoweringContext& lowering, llvm::IRBuilder<>& builder, LocalsMap& locals
		):
			  ctx(ctx),
			  lowering(lowering),
			  builder(builder),
			  locals(locals) {}

		LoweredExpr lower(const pst::ExprElement& expr);

		void visitExprNumericValue(pst::Access<pst::expr::ExprNumericValue> lit) override;
		void visitKeywordLiteral(pst::Access<pst::expr::KeywordLiteral> kw) override;
		void visitIdentifierLiteral(pst::Access<pst::expr::IdentifierLiteral> ident) override;
		void visitBinaryOperator(pst::Access<pst::expr::BinaryOperator> op) override;
		void visitComparisonChain(pst::Access<pst::expr::ComparisonChain> chain) override;
		void visitRoundExpr(pst::Access<pst::expr::RoundExpr> round) override;
		void visitAssignment(pst::Access<pst::expr::Assignment> assignment) override;
		void visitCall(pst::Access<pst::expr::Call> call) override;
		void visitChainExpr(pst::Access<pst::expr::ChainExpr> chain) override;

	private:
		/// Turns a resolved symbol into a LoweredExpr: variables load from their slot,
		/// constants materialize as immediates, functions/namespaces become symbol refs.
		LoweredExpr symbolRefToExpr(sym::SymbolID sym_id, base::StrID name);

		/// Emits a call to `callee` (which must be a function symbol reference).
		LoweredExpr          emitCall(const LoweredExpr& callee, pst::Access<pst::expr::Call> call);
		Context&             ctx;
		LlvmLoweringContext& lowering;
		llvm::IRBuilder<>&   builder;
		LocalsMap&           locals;

		LoweredExpr result{};
	};
}
