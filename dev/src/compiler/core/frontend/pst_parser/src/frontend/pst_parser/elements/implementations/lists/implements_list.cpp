#include "../../hierarchy/lists/implements_list.hpp"

#include "../../hierarchy/expressions/ternary.hpp"
#include "impl_template.hpp"

namespace pst {
	MBox<ImplementsList> ImplementsList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			ImplementsElementExprHolder,
			ImplementsList,
			true,   // empty list not allowed
			false,  // trailing separator not allowed
			lexer::Token::BracketType::None,
			internal::Conditions::isComma,
			internal::Conditions::isBlockGroup,
			internal::NameGetters::inheritanceList>(state);
	}
}
