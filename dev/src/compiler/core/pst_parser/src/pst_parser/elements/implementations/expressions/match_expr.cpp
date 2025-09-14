#include "../../hierarchy/expressions/match_expr.hpp"  // IWYU pragma: keep

#include "../../hierarchy/expressions/ternary.hpp"     // IWYU pragma: keep
#include "../../hierarchy/not_statements/match_case.hpp"
#include "preamble.hpp"

namespace pst::expr {
	class MatchRoundBracketError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected round bracket group.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		MatchRoundBracketError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class NotACaseExpression final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Not a case expression";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		NotACaseExpression(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class MatchCurlyBracketError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected round bracket group.";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		MatchCurlyBracketError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	MBox<ExprElement> MatchExpr::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		if (!state[0].is(Keyword::Match)) return Lower::parse(state, length);

		auto position = state.getPosition();
		auto out      = makeBox<MatchExpr>(position);

		if (!assertStmtChoice<MatchExpr>(state, state[0].is(Keyword::Match))) return nullptr;
		state.parse(out).one(Keyword::Match);

		if (!state[0].isBracketGroup(Token::BracketType::Round)) {
			state.log(makeBox<MatchRoundBracketError>(state.getPosition()));
			return nullptr;
		}
		state.goDown();
		state.parse(out).one(&out->value_to_match);
		state.goUpAndSkip();

		if (!state[0].isBracketGroup(Token::BracketType::Curly)) {
			state.log(makeBox<MatchRoundBracketError>(state.getPosition()));
			return nullptr;
		}
		state.parse(out).goDown();

		while (true) {
			if (state[0].is(Keyword::Case)) {
				MBox<MatchCase> match_case;
				state.parse(out).one(&match_case);
				if (match_case) {
					out->cases.emplace_back(nullptr);
					state.parse(out).assign(&out->cases.back(), std::move(match_case));
				}
			} else {
				break;
			}
		}
		// A non-case in a match expression.
		if (state.notEmpty()) state.log(makeBox<NotACaseExpression>(state.getPosition()));

		state.parse(out).goUpAndSkip();
		state.parse(out).one(Special::Semicolon);
		return out;
	}

	void MatchExpr::dprint(std::ostream& out) const {
		out << "{";
		out << R"("node_type": "Match Expr",)";
		out << R"("value_to_match": )";
		nullAwareDprint(value_to_match, out);
		out << R"(, "cases": [)";

		bool first_case = true;
		for (const auto& match_case: cases) {
			if (!first_case) out << ", ";
			nullAwareDprint(match_case, out);
			first_case = false;
		}
		out << "]}";
	}

	void MatchExpr::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitMatchExpr(*this);
	}

	void MatchExpr::calcElementPathsRecursive() {
		calcIndexedListChildPath<MatchCase>({ cases }, getElementPath());
	}
}
