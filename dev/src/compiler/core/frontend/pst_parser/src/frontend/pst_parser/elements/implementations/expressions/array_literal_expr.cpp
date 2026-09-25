#include "../../hierarchy/expressions/array_literal_expr.hpp"

#include "preamble.hpp"

namespace pst::expr {
	CLONE_SUB_ELEMENTS_DEF(ArrayLiteralExpr, list);

	MBox<ExprElement> ArrayLiteralExpr::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		auto out = makeBox<ArrayLiteralExpr>(state);
		PARSE().one(&out->list);

		PST_RETURN out;
	}

	void ArrayLiteralExpr::dprint(std::ostream& out) const {
		out << "{";

		out << R"("list": )";
		nullAwareDprint(list, out);

		out << "}";
	}

	HashAlg& ArrayLiteralExpr::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void ArrayLiteralExpr::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitArrayLiteralExpr(*this);
	}
}
