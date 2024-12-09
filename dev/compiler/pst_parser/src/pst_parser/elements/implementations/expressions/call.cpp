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
			// This should almost never happen
			state.log(base::make_unique<BadCallError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
		}

		auto out = makeBox<Call>(state.getPosition());

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

	void Call::acceptVisitor(PstExprVisitor& visitor) const { visitor.visitCall(*this); }
}
