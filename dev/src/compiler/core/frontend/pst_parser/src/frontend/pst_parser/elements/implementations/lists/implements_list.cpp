#include "../../hierarchy/lists/implements_list.hpp"

#include "frontend/pst_parser/elements/hierarchy/expr_holders.hpp"
#include "frontend/pst_parser/elements/hierarchy/expressions/ternary.hpp"
#include "impl_template.hpp"

namespace pst {
	namespace {
		bool implementsElementEnd(const LangParserState& state, i64 fwd = 0) {
			return state[fwd].is(Special::Semicolon) || state[fwd].is(NamedOperator::Assign)
			    || state[fwd].is(Special::Comma) || internal::Conditions::isBlockGroup(state, fwd);
		}
	}

	MBox<ExprElement> ExprParserHelper::parseImplementsList(LangParserState& state) {
		return expr::parseUntil<expr::Ternary, implementsElementEnd>(state);
	}

	MBox<ImplementsList> ImplementsList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			ImplementsListExprHolder,
			ImplementsList,
			true,
			lexer::Token::BracketType::None,
			internal::Conditions::isComma,
			internal::Conditions::isBlockGroup,
			internal::NameGetters::inheritanceList>(state);
	}
}
