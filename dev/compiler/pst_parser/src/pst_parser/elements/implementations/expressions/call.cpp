#include "preamble.hpp"

namespace pst::expr {
	class BadCallError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected single call expression";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		BadCallError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	MBox<ExprElement> Call::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		if (not(length == 1
		        && (state[0].isBracketGroup(lexer::Token::Round)
		            || state[0].isBracketGroup(lexer::Token::Square)))) {
			// This should (probably) never happen with how it's called by the parser
			state.log(makeBox<BadCallError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
		}

		auto out = makeBox<Call>(state.getPosition());
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

	void Call::acceptExprVisitor(PstExprVisitor& visitor) const { visitor.visitCall(*this); }
}
