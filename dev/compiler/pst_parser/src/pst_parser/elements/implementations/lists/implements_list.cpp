#include "impl_template.hpp"

#include "../../hierarchy/not_statements.hpp"

namespace pst {
	ParserRef<ImplementsList> ImplementsList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			Expr,
			ImplementsList,
			true,
			lexer::Token::BracketType::None,
			detail::Conditions::isComma,
			detail::Conditions::isBlockGroup,
			detail::NameGetters::inheritanceList>(state);
	}
}
