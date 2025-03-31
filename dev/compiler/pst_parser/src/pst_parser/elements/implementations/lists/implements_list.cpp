#include "../../hierarchy/not_statements.hpp"
#include "impl_template.hpp"

namespace pst {
	MBox<ImplementsList> ImplementsList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			UniversalExprHolder,
			ImplementsList,
			true,
			lexer::Token::BracketType::None,
			detail::Conditions::isComma,
			detail::Conditions::isBlockGroup,
			detail::NameGetters::inheritanceList>(state);
	}
}
