#include "preamble.hpp"

namespace pst::expr {
	class BadRoundExprError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected single block expression";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		BadRoundExprError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	MBox<ExprElement> RoundExpr::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		if (not(length == 1 && state[0].isBracketGroup(lexer::Token::Round))) {
			// This should (probably) never happen with how it's called by the parser
			state.log(base::make_unique<BadRoundExprError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
		}

		auto out = makeBox<RoundExpr>(state.getPosition());

		state.parse(out).goDown();
		state.parse(out).with(&out->expr, Comma::parse, (i64) state.ctokens().size());
		state.parse(out).goUpAndSkip();

		return out;
	}

	void RoundExpr::dprint(std::ostream& out) const {
		out << "{";

		out << R"("inner_expr": )";
		nullAwareDprint(expr, out);

		out << "}";
	}

	void RoundExpr::acceptExprVisitor(PstExprVisitor& visitor) const { visitor.visitRoundExpr(*this); }
}
