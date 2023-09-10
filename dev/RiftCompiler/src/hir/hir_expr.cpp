#include "hir_expr.hpp"
#include <base/exceptions.hpp>

// @Placeholder

namespace hir {

	class SymbolExpr: public Expression {};

	class LiteralExpr: public Expression {};

	class OperatorExpr: public Expression {};

	// all other types like: lambda

	ExpressionRef Expression::makeExpr(pst::ParserCBorrowRef<pst::Expr> pst_expr) {
		
		// temporary:

		if (pst_expr->elements.size() == 1) {
			// @TODO
			return nullptr;
		}
		else {
			throw base::NotYetImplemented("Make Hir Expr for longer expressions");
		}

	}
};
