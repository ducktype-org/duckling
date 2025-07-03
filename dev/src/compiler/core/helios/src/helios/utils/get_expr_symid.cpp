#include "get_expr_symid.hpp"

#include <helios/hout/visitors.hpp>

#include <base/optional.hpp>

namespace compiler::helios {


	struct HoutExprSymbolVisitor final: public code::HoutExprVisitorEmpty {
		base::Optional<SymID> symbol;

		void visitIdentifierExpr(const code::IdentifierExpr& val) override { symbol = val.symbol; }

		void visitParenthesisExpr(const code::ParenthesisExpr& val) override {
			val.inner->acceptVisitor(*this);
		}
	};

	/**
	 * Wrapper around the HoutExprSymbolVisitor to get the symbol ID from an expression.
	 */
	base::Optional<SymID> getIdentifierExprSymID(CRef<code::Expr> expr) {
		HoutExprSymbolVisitor visitor;
		expr->acceptVisitor(visitor);
		return visitor.symbol;
	}
}
