#include "../../hierarchy/not_statements/round_group_expression.hpp"

#include "preamble.hpp"

namespace pst {
	class RoundExprStartError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected an expression starting with `(`.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		RoundExprStartError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	MBox<RoundGroupExpr> RoundGroupExpr::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<RoundGroupExpr>(position);

		if (!state[0].isBracketGroup(Token::BracketType::Round)) {
			state.log(makeBox<RoundExprStartError>(state.getPosition()));
			return nullptr;
		}

		state.parse(out).goDown();
		if (state.notEmpty()) state.parse(out).one(&out->expr);
		state.parse(out).goUpAndSkip();

		return out;
	}

	void RoundGroupExpr::dprint(std::ostream& out) const { nullAwareDprint(expr, out); }

	LangElement::HashAlg& RoundGroupExpr::calcStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}
}
