#include "hir_expr.hpp"

// @Placeholder

namespace hir {

	class SymbolExpr: public Expression {};

	class LiteralExpr: public Expression {};

	class OperatorExpr: public Expression {};

	// all other types like: lambda

	ExpressionRef Expression::makeExpr(pst::ParserCBorrowRef<pst::Expr> pst_expr) {
		// @TODO

		return nullptr;
	}
};
