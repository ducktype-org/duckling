#include "elements_implementation.hpp"

namespace pst {
	// @TODO: make better
	ParserRef<Fun> Fun::parse(RiftParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<Fun>(position);

		RIFT_ASSERT(state[0].is(Keyword::Fun), position.genStr("bad statement choice"));

		out->addKeyword(state.getPosition());

		parseAll(state, Keyword::Fun, &out->name, &out->params);
		if (state.tryEat(Operator::SingleArrow)) parseOne(state, &out->rets);
		while (state.notEmpty() and !state[0].isBracketGroup(Token::BracketType::Curly))
			state.tokens().skip();
		parseOne(state, &out->body);

		out->setLastToken(state.getPosition(-1));

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
}
