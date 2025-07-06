#include "../../hierarchy/expressions/binary_operator.hpp"

#include "preamble.hpp"

namespace pst::expr {
	void BinaryOperator::dprint(std::ostream& out) const {
		out << "{";

		out << R"("left_expr": )";
		nullAwareDprint(left, out);
		out << R"(, "operator": )";
		tpc::nullAwareDprint(op, out);
		out << R"(, "right_expr": )";
		nullAwareDprint(right, out);

		out << "}";
	}

	void BinaryOperator::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitBinaryOperator(*this);
	}

	AccessLocked<ExprElement> BinaryOperator::getLeftOperand() const { return left.give(); }

	AccessLocked<ExprElement> BinaryOperator::getRightOperand() const { return right.give(); }

	lexer::Operator BinaryOperator::getOperator() const { return op; }
}
