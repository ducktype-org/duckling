#include "preamble.hpp"

namespace pst {
	ParserRef<Struct> Struct::parse(RiftParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<Struct>(position);

		if (!assertStmtChoice<Struct>(state, state[0].is(Keyword::Struct))) return nullptr;

		state.parse(out).all(Keyword::Struct, &out->name);

		if (state.parse(out).tryEat(Operator::Colon)) state.parse(out).one(&out->bases);

		state.parse(out).one(&out->body);

		return out;
	}

	void Struct::dprint(std::ostream& out) const {
		out << "{\"Struct\": {\"name\":";
		nullAwareDprint(name, out);

		out << R"(,"base_classes":)";
		nullAwareDprint(bases, out);
		out << ",";

		out << R"("body": )";
		nullAwareDprint(body, out);

		out << "}}";
	}

	void Struct::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitStruct(*this); }
}
