#include "preamble.hpp"

namespace pst {
	MBox<NonClassStmt> NonClassStmt::parse(LangParserState& state, const ClassContext& ctx) {
		auto position = state.getPosition();
		auto out      = makeBox<NonClassStmt>(position, ctx);

		out->parseSpecifiers(state);

		state.parse(out).one(&out->inner_stmt);

		return out;
	}

	void NonClassStmt::dprint(std::ostream& out) const {
		out << "{";

		out << R"("internal stmt": )";
		nullAwareDprint(inner_stmt, out);

		out << "}";
	}

	void NonClassStmt::acceptVisitor(PstVisitor& visitor) const {
		inner_stmt.internal()->acceptVisitor(visitor);
	}
}
