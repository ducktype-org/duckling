#include "../../hierarchy/class_elements/constructor.hpp"

#include "../../hierarchy/lists.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	MBox<Constructor> Constructor::parse(LangParserState& state, const ClassContext& ctx) {
		auto position = state.getPosition();
		auto out      = makeBox<Constructor>(position, ctx);

		out->parseSpecifiers(state);

		state.parse(out).eatOne();

		if (state[0].isBracketGroup(Token::Round))
			out->kind = tpc::Identifier{ .value = base::StrID("create") };
		else {
			tpc::Identifier ident;
			state.parse(out).all(NamedOperator::Period, &ident);
			out->kind = ident;
		}

		state.parse(out).one(&out->params);
		if (state.parse(out).tryEat(NamedOperator::Colon)) state.parse(out).one(&out->inits);
		state.parse(out).all(NamedOperator::Assign, &out->body);

		return out;
	}

	void Constructor::dprint(std::ostream& out) const {
		out << "{";
		out << "\"name\":";
		out << "\"" << getName().strView() << "\"";
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
