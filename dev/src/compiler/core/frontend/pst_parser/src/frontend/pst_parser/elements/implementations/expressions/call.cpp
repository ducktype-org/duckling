#include "../../hierarchy/expressions/call.hpp"

#include "preamble.hpp"

#include <diagnostic_interactive/message.hpp>

namespace pst::expr {
	class BadCallError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_call_error" };
		}

	public:
		BadCallError(dia::SourcePosition pos): dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	MBox<ExprElement> Call::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		if (not(length == 1
		        && (state[0].isBracketGroup(lexer::Token::Round)
		            || state[0].isBracketGroup(lexer::Token::Square)))) {
			// This should (probably) never happen with how it's called by the parser
			state.logInt(makeBox<BadCallError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
		}

		auto out  = makeBox<Call>(state.getPosition());
		out->type = state[0].getBracketType();

		state.parse(out).goDown();
		state.parse(out).one(&out->args);
		state.parse(out).goUpAndSkip();

		return out;
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

	LangElement::HashAlg& Call::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, type);
		return partial_hash;
	}

	void Call::acceptExprVisitor(PstExprVisitor& visitor) const { visitor.visitCall(*this); }

	lexer::Token::BracketType Call::getType() const { return type; }
}
