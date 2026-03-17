#include "../../hierarchy/expressions/match_expr.hpp"  // IWYU pragma: keep

#include "../../hierarchy/expressions/ternary.hpp"     // IWYU pragma: keep
#include "../../hierarchy/not_statements/match_case.hpp"
#include "preamble.hpp"

namespace pst::expr {

	MBox<ExprElement> MatchExpr::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		if (!state[0].is(Keyword::Match)) return Lower::parse(state);

		auto out = makeBox<MatchExpr>(state);

		if (!assertStmtChoice<MatchExpr>(state, state[0].is(Keyword::Match))) return nullptr;
		PARSE().one(Keyword::Match);

		if (!state[0].isBracketGroup(Token::BracketType::Round)) {
			state.logInt(makeBox<MatchRoundBracketError>(state.getPosition()));
			return nullptr;
		}

		PARSE().goDown();
		PARSE().one(&out->value_to_match);
		PARSE().goUpAndSkip();

		if (!state[0].isBracketGroup(Token::BracketType::Curly)) {
			state.logInt(makeBox<MatchCurlyBracketError>(state.getPosition()));
			return nullptr;
		}
		PARSE().goDown();

		PST_WHILE(true) {
			if (state[0].is(Keyword::Case)) {
				MBox<MatchCase> match_case;
				PARSE().one(&match_case);
				if (match_case) {
					out->cases.emplace_back(nullptr);
					PARSE().assign(&out->cases.back(), std::move(match_case));
				}
			} else {
				break;
			}
		}
		// A non-case in a match expression.
		if (state.notEmpty()) state.logInt(makeBox<NotACaseExpression>(state.getPosition()));

		PARSE().goUpAndSkip();
		PST_RETURN out;
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

	HashAlg& MatchExpr::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, cases.size());
		return partial_hash;
	}

	void MatchExpr::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitMatchExpr(*this);
	}

	void MatchExpr::calcElementPathHashRecursive() {
		calcNamedChildPath(value_to_match, getElementPathHash());
		auto path = getElementPathHash();
		calcIndexedListChildPath<MatchCase>({ cases }, { path, "cases" });
	}
}
