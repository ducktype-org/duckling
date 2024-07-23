#include "elements_implementation.hpp"
#include "pst_parser/pst_visitor.hpp"

namespace pst {
	ParserRef<Namespace> Namespace::parse(RiftParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<Namespace>(position);

		if (!assertStmtChoice<Namespace>(state, state[0].is(Keyword::Namespace))) return nullptr;

		state.parse(out).all(Keyword::Namespace, &out->name, &out->body);

		return out;
	}

	void Namespace::dprint(std::ostream& out) const {
		out << "{\"Namespace\": {";

		out << R"("name": )";
		nullAwareDprint(name, out);

		// @TODO: change to body in print:
		out << R"(, "block": )";
		nullAwareDprint(body, out);
		out << "}}";
	}

	void Namespace::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitNamespace(*this); }
}
