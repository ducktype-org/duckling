#include "../../hierarchy/expressions/prefix_operator.hpp"

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

	HashAlg& PrefixOperator::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, op);
		return partial_hash;
	}

	lexer::Operator PrefixOperator::getOperator() const { return op; }

	AccessLocked<ExprElement> PrefixOperator::getExpr() const { return expr.give(); }
}
