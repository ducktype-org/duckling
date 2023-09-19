#include "automatic.hpp"

namespace tpc {

	void parseOne(ParserState& state, Keyword key) {
		if (!state.tryEat(key)) {
			state.fail(-1,
			           "expected keyword `" + rift_def::keywordToStr(key).str() + "` after here");
		}
	}

	void parseOne(ParserState& state, Special spec) {
		if (!state.tryEat(spec)) {
			state.fail(-1,
			           "expected special `" + rift_def::specialToStr(spec).str() + "` after here");
		}
	}

	void parseOne(ParserState& state, Operator op) {
		if (!state.tryEat(op)) {
			state.fail(-1,
			           "expected operator `" + rift_def::operatorToStr(op).str() + "` after here");
		}
	}

	void parseOne(ParserState& state, Identifier* ident) {
		if (!state.ctokens().peek().isIdentifier()) state.fail(-1, "expected identifier");
		ident->value = state.tokens().next().getValue();
	}

	void parseOne(ParserState& state, OptionalIdentifier* ident) {
		if (state.ctokens().peek().isIdentifier()) ident->value = state.tokens().next().getValue();
	}

	void identifierDprint(base::StrId value, std::ostream& out) {
		// @TODO: change Name to Identifier
		out << "{\"Name\": ";
		if (value.isBad())
			out << "\"BAD_NAME\"";
		else
			out << "\"" << value.strView() << "\"";
		out << "}";
	}

	void nullAwareDprint(Identifier ident, std::ostream& out) {
		identifierDprint(ident.value, out);
	}

	void nullAwareDprint(OptionalIdentifier ident, std::ostream& out) {
		if (ident.value.has_value())
			identifierDprint(ident.value.value(), out);
		else
			out << "\"<ANONYMOUS>\"";
	}

}
