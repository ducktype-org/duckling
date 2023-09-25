#pragma once

#include <pst_parser/pst.hpp>  // Expr, @TODO: separate expr from rest?

// This code is a temporary setup
// @Placeholder

namespace hir {

	class Expression;

	using ExpressionRef = base::unique_ptr<Expression>;

	class Expression {
		Expression();

	public:
		// in the future this will probably require some „grammar context”
		// in the future this will require AnalysisState to log errors
		ExpressionRef makeExpr(pst::ParserCBorrowRef<pst::Expr>);
		// eval
	};

}
