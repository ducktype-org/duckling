#include "../../hierarchy/expr_holders.hpp"  // IWYU pragma: keep
#include "../../hierarchy/statements/expr_stmt.hpp"
#include "preamble.hpp"

namespace pst {
	MBox<ExprStmt> ExprStmt::parse(LangParserState& state) {
		auto out = makeBox<ExprStmt>(state.getPosition());
		state.parse(out).one(&out->expr);
		return out;
	}

	void ExprStmt::dprint(std::ostream& out) const { nullAwareDprint(expr, out); }

	u64 ExprStmt::calcStableHash(HashAlg& partial_hash) const {
		return partial_hash.finalize();
	}

	void ExprStmt::acceptVisitor(PstVisitor& visitor) const { visitor.visitExprStmt(*this); }
}
