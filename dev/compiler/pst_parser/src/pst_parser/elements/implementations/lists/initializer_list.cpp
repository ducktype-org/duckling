#include "impl_template.hpp"

#include "../../hierarchy/not_statements.hpp"

namespace pst {
	MBox<InitList> InitList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			UniversalExprHolder,
			InitList,
			false,
			lexer::Token::BracketType::None,
			detail::Conditions::isComma,
			detail::Conditions::isAssign,
			detail::NameGetters::classInitList>(state);
	}
}
