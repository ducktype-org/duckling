#include "../../hierarchy/lists.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	MBox<Constructor> Constructor::parse(LangParserState& state, const ClassContext& ctx) {
		auto position = state.getPosition();
		auto out      = makeBox<Constructor>(position, ctx);

		out->parseSpecifiers(state);

		state.parse(out).eatOne();

		if (state[0].isBracketGroup(Token::Round))
			out->kind = { base::StrID("create") };
		else
			state.parse(out).all(NamedOperator::Period, &out->kind);

		state.parse(out).one(&out->params);
		if (state.parse(out).tryEat(NamedOperator::Colon)) state.parse(out).one(&out->inits);
		state.parse(out).all(NamedOperator::Assign, &out->body);

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

	void Constructor::acceptVisitor(PstVisitor& visitor) const { visitor.visitConstructor(*this); }
}
