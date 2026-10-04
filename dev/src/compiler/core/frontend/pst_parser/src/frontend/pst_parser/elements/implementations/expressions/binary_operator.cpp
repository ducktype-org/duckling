#include "../../hierarchy/expressions/binary_operator.hpp"

#include "preamble.hpp"

namespace pst::expr {
	CLONE_SUB_ELEMENTS_DEF(BinaryOperator, left, op, right);

	void BinaryOperator::dprint(std::ostream& out) const {
		out << "{";

		out << R"("left_expr": )";
		nullAwareDprint(left, out);
		out << R"(, "operator": )";
		nullAwareDprint(op, out);
		out << R"(, "right_expr": )";
		nullAwareDprint(right, out);

		out << "}";
	}

	HashAlg& BinaryOperator::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void BinaryOperator::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitBinaryOperator(*this);
	}

	AccessLocked<ExprElement> BinaryOperator::getLeftOperand() const { return left.give(); }

	AccessLocked<ExprElement> BinaryOperator::getRightOperand() const { return right.give(); }

	AccessLocked<OperatorWrapper> BinaryOperator::getOperator() const { return op.give(); }
}
