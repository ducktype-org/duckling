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

	void SuffixOperator::acceptVisitor(PstExprVisitor& visitor) const { visitor.visitSuffixOperator(*this); }
}
