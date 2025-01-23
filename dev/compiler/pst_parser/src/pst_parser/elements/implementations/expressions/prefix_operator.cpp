#include "preamble.hpp"

namespace pst::expr {
	void PrefixOperator::dprint(std::ostream& out) const {
		out << "{";

		out << R"("operator": )";
		tpc::nullAwareDprint(op, out);
		out << R"(, "expression": )";
		nullAwareDprint(expr, out);

		out << "}";
	}

	void PrefixOperator::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitPrefixOperator(*this);
	}
}
