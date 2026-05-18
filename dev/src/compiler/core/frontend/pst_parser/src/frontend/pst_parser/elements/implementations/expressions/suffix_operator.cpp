#include "../../hierarchy/expressions/suffix_operator.hpp"

#include "preamble.hpp"

namespace pst::expr {
	void SuffixOperator::dprint(std::ostream& out) const {
		out << "{";

		out << R"("expression": )";
		nullAwareDprint(expr, out);
		out << R"(, "operator": )";
		nullAwareDprint(op, out);

		out << "}";
	}

	HashAlg& SuffixOperator::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void SuffixOperator::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitSuffixOperator(*this);
	}

	AccessLocked<OperatorWrapper> SuffixOperator::getOperator() const { return op.give(); }

	AccessLocked<ExprElement> SuffixOperator::getExpr() const { return expr.give(); }
}
