#include "../../hierarchy/expressions/call.hpp"

#include "preamble.hpp"

namespace pst::expr {
	CLONE_SUB_ELEMENTS_DEF(Call, args);

	MBox<ExprElement> Call::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		i64 length = base::safeIntConv<i64>(state.ctokens().size());

		if (not(length == 1
		        && (state[0].isBracketGroup(lexer::Token::Round)
		            || state[0].isBracketGroup(lexer::Token::Square)))) {
			// This should (probably) never happen with how it's called by the parser
			state.logInt(makeBox<BadCallError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
		}

		auto out  = makeBox<Call>(state);
		out->type = state[0].getBracketType();

		PARSE().goDown();
		PARSE().one(&out->args);
		PARSE().goUpAndSkip();

		PST_RETURN out;
	}

	void Call::dprint(std::ostream& out) const {
		out << "{";

		if (type == lexer::Token::Round)
			out << R"--("type": "()")--";
		else
			out << R"--("type": "[]")--";
		out << R"(, "arguments": )";
		nullAwareDprint(args, out);

		out << "}";
	}

	HashAlg& Call::addElementDataToStableHash(HashAlg& partial_hash) const {
		hashing::addToHash(partial_hash, type);
		return partial_hash;
	}

	void Call::acceptExprVisitor(PstExprVisitor& visitor) const { visitor.visitCall(*this); }

	lexer::Token::BracketType Call::getType() const { return type; }
}
