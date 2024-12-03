#include "preamble.hpp"

namespace pst {
	MBox<Namespace> Namespace::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = box<Namespace>(position);

		if (!assertStmtChoice<Namespace>(state, state[0].is(Keyword::Namespace))) return nullptr;

		state.parse(out).all(Keyword::Namespace, &out->name, &out->body);

		return out;
	}

	void Namespace::dprint(std::ostream& out) const {
		out << "{";

		out << R"("name": )";
		nullAwareDprint(name, out);

		// @TODO: change to body in print:
		out << R"(, "block": )";
		nullAwareDprint(body, out);

		out << "}";
	}

	void Namespace::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitNamespace(*this); }
}
