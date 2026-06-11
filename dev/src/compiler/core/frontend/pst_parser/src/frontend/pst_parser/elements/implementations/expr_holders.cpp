#include "../hierarchy/expr_holders.hpp"

#include "../hierarchy/expressions/assignment.hpp"
#include "../hierarchy/expressions/comma.hpp"
#include "../hierarchy/expressions/ternary.hpp"
#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(ExprHolder, expr);

	void ExprHolder::dprint(std::ostream& out) const {
		out << "{";

		out << R"("expr":)";
		nullAwareDprint(expr, out);

		out << "}";
	}

	HashAlg& ExprHolder::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	bool ExprParserHelper::untilUniversalEnd(const TokenStream& state, i64 fwd) {
		return state[fwd].is(Special::Comma) || state[fwd].is(Special::Semicolon)
		    || ExprClassify::isAssignment(state, fwd)
		    || internal::Conditions::isBlockGroup(state, fwd);
	}

	bool ExprParserHelper::untilUniversalAllowBlockEnd(const TokenStream& state, i64 fwd) {
		return state[fwd].is(Special::Comma) || state[fwd].is(Special::Semicolon)
		    || ExprClassify::isAssignment(state, fwd);
	}

	bool ExprParserHelper::untilUniversalAllowCommaEnd(const TokenStream& state, i64 fwd) {
		return state[fwd].is(Special::Semicolon) || ExprClassify::isAssignment(state, fwd)
		    || internal::Conditions::isBlockGroup(state, fwd);
	}

	bool ExprParserHelper::untilUniversalAllowCommaAndBlockEnd(const TokenStream& state, i64 fwd) {
		return state[fwd].is(Special::Semicolon) || ExprClassify::isAssignment(state, fwd);
	}

	bool ExprParserHelper::untilSemicolon(const TokenStream& state, i64 fwd) {
		return state[fwd].is(Special::Semicolon);
	}

	MBox<ExprElement> ExprParserHelper::parseAssignment(LangParserState& state) {
		return expr::Assignment::parse(state);
	}

	MBox<ExprElement> ExprParserHelper::parseComma(LangParserState& state) {
		return expr::Comma::parse(state);
	}

	MBox<ExprElement> ExprParserHelper::parseTernary(LangParserState& state) {
		return expr::Ternary::parse(state);
	}

}
