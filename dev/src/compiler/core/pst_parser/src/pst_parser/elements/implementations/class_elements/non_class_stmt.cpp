#include "../../hierarchy/class_elements/non_class_stmt.hpp"

#include "preamble.hpp"

namespace pst {
	MBox<NonClassStmt> NonClassStmt::parse(LangParserState& state, const ClassContext& ctx) {
		auto position = state.getPosition();
		auto out      = makeBox<NonClassStmt>(position, ctx);

		out->parseSpecifiers(state);

		Keyword as_keyword = state[0].asKeyword();
		CORE_ASSERT(
			as_keyword == Keyword::Alias || as_keyword == Keyword::Using,
			"Bad starting keyword in NonClassStmt."
		);

		state.parse(out).one(&out->inner_stmt);

		return out;
	}

	void NonClassStmt::dprint(std::ostream& out) const {
		out << "{";

		out << R"("internal stmt": )";
		nullAwareDprint(inner_stmt, out);

		out << "}";
	}

	LangElement::HashAlg& NonClassStmt::calcStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void NonClassStmt::acceptVisitor(PstVisitor& visitor) const {
		inner_stmt.internal()->acceptVisitor(visitor);
	}
}
