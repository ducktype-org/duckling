#include "../../hierarchy/lists/initializer_list.hpp"

#include "impl_template.hpp"

namespace pst {
	MBox<InitList> InitList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			UniversalExprHolder,
			InitList,
			false,
			lexer::Token::BracketType::None,
			internal::Conditions::isComma,
			internal::Conditions::isAssign,
			internal::NameGetters::classInitList>(state);
	}
}
