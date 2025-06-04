#include "../../hierarchy/lists/initializer_list.hpp"

#include "impl_template.hpp"

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
