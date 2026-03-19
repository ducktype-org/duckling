#include "../../hierarchy/expr_holders.hpp"  // IWYU pragma: keep
#include "../../hierarchy/statements/expr_stmt.hpp"
#include "preamble.hpp"

namespace pst {
	MBox<ExprStmt> ExprStmt::parse(LangParserState& state) {
		auto out = makeBox<ExprStmt>(state);
		PARSE().one(&out->expr);
		PST_RETURN out;
	}

	void ExprStmt::dprint(std::ostream& out) const { nullAwareDprint(expr, out); }

	HashAlg& ExprStmt::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void ExprStmt::acceptVisitor(PstVisitor& visitor) const { visitor.visitExprStmt(*this); }
}
