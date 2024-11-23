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

	void BinaryOperator::acceptVisitor(PstExprVisitor& visitor) const {
		visitor.visitBinaryOperator(*this);
	}
}
