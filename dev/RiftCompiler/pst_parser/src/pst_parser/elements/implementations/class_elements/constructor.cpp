#include "preamble.hpp"

namespace pst {
	ParserRef<Constructor> Constructor::parse(RiftParserState& state, const ClassContext& ctx) {
		auto position = state.getPosition();
		auto out      = makeRef<Constructor>(position, ctx);

		out->parseSpecifiers(state);

		state.parse(out).eatOne();

		if (state[0].isBracketGroup(Token::Round))
			out->kind = { base::StrId("create") };
		else
			state.parse(out).all(Operator::Period, &out->kind);

		state.parse(out).one(&out->params);
		if (state.parse(out).tryEat(Operator::Colon)) state.parse(out).one(&out->inits);
		state.parse(out).one(&out->body);

		return out;
	}

	void Constructor::dprint(std::ostream& out) const {
		out << "{";
		out << "\"name\":";
		nullAwareDprint(kind, out);
		out << ",\"params\":";
		nullAwareDprint(params, out);
		out << ",\"inits\":";
		nullAwareDprint(inits, out);
		out << ",\"body\":";
		nullAwareDprint(body, out);
		out << "}";
	}

	void Constructor::acceptVisitor(PstStmtVisitor& visitor) const {
		visitor.visitConstructor(*this);
	}
}
