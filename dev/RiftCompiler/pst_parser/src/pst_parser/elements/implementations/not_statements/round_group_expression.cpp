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

	ParserRef<RoundGroupExpr> RoundGroupExpr::parse(RiftParserState& state) {
		auto position = state.getPosition();
		auto out      = makeRef<RoundGroupExpr>(position);

		if (!state[0].isBracketGroup(Token::BracketType::Round)) {
			state.log(base::make_unique<RoundExprStartError>(state.getPosition()));
			return nullptr;
		}

		state.parse(out).goDown();
		if (state.notEmpty()) state.parse(out).with<Expr>(&out->expr, Expr::parse, true);
		state.parse(out).goUpAndSkip();

		return out;
	}

	void RoundGroupExpr::dprint(std::ostream& out) const {
		out << "{\"RoundGroupExpr\": ";
		nullAwareDprint(expr, out);
		out << " }";
	}

	void RoundGroupExpr::semPrint(std::ostream& out) const {
		out << "{\"RoundGroupExpr\": ";
		position.semPrint(out);
		out << R"(,"semanticTokenType": "namespace",)"; // TODO: maybe needs a change?
		nullAwareSemanticTokenPrint(expr, out);
		out << " }";
	}
}
