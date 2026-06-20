#include "../../hierarchy/lists/initializer_list.hpp"

#include "impl_template.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(InitList, elements);

	MBox<InitList> InitList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			UniversalExprHolder,
			InitList,
			false,  // empty list allowed
			false,  // trailing separator not allowed
			lexer::Token::BracketType::None,
			internal::Conditions::isComma,
			internal::Conditions::isAssign,
			internal::NameGetters::classInitList>(state);
	}
}
