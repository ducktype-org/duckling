#include "elements_implementation.hpp"
#include "pst_parser/pst_visitor.hpp"

namespace pst {
	// @TODO: make better
	ParserRef<Fun> Fun::parse(RiftParserState& state) {
		auto position = state.ctokens().peek().getPosition();
		auto out      = makeRef<Fun>(position);

		RIFT_ASSERT(state.ctokens().is(Keyword::Fun), position.genStr("bad statement choice"));

		parseAll(state, Keyword::Fun, &out->name, &out->params);
		if (state.tryEat(Operator::SingleArrow)) parseOne(state, &out->rets);
		while (state.notEmpty() and !state.ctokens().isBracketGroup(Token::BracketType::Curly))
			state.tokens().skip();
		parseOne(state, &out->body);
		return out;
	}

	void Fun::dprint(std::ostream& out) const {
		out << "{\"Fun\": { ";
		out << "\"name\": ";
		nullAwareDprint(name, out);
		out << ", \"params\":";
		nullAwareDprint(params, out);
		out << ", \"rets\":";
		nullAwareDprint(rets, out);
		out << ", \"body\":";
		nullAwareDprint(body, out);
		out << " } }";
	}

	void Fun::acceptVistior(PstStmtVisitor& visitor) const { visitor.visitFun(*this); }
}
