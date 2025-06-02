#include "../../hierarchy/expressions/suffix_operator.hpp"

#include "preamble.hpp"

namespace pst::expr {
	void SuffixOperator::dprint(std::ostream& out) const {
		out << "{";

		out << R"("expression": )";
		nullAwareDprint(expr, out);
		out << R"(, "operator": )";
		tpc::nullAwareDprint(op, out);

		out << "}";
	}

	void SuffixOperator::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitSuffixOperator(*this);
	}

	lexer::Operator SuffixOperator::getOperator() const { return op; }

	AccessLocked<ExprElement> SuffixOperator::getExpr() const { return expr.give(); }
}
