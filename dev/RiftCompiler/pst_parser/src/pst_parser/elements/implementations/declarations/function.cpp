#include "preamble.hpp"

namespace pst {
	// @TODO: make better
	ParserRef<Fun> Fun::parse(RiftParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<Fun>(position);

		if (!assertStmtChoice<Fun>(state, state[0].is(Keyword::Fun))) return nullptr;

		state.parse(out).all(Keyword::Fun, &out->name);
		state.parse(out).with<ParamList>(&out->params, ParamList::parse);
		if (state.parse(out).tryEat(Operator::SingleArrow)) state.parse(out).with<Expr>(&out->ret, Expr::parse, false);
		while (state.notEmpty() and !state[0].isBracketGroup(Token::BracketType::Curly))
			state.tokens().skip();
		state.parse(out).one(&out->body);

		return out;
	}

	void Fun::dprint(std::ostream& out) const {
		out << "{\"Fun\": { ";
		out << "\"name\": ";
		nullAwareDprint(name, out);
		out << ", \"params\":";
		nullAwareDprint(params, out);
		out << ", \"rets\":";
		nullAwareDprint(ret, out);
		out << ", \"body\":";
		nullAwareDprint(body, out);
		out << " } }";
	}

	void Fun::acceptVisitor(PstStmtVisitor& visitor) const { visitor.visitFun(*this); }
}
