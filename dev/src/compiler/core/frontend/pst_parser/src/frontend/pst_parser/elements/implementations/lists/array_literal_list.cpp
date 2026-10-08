#include "../../hierarchy/lists/array_literal_list.hpp"

#include "impl_template.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(ArrayLiteralList, elements);

	MBox<ArrayLiteralList> ArrayLiteralList::parse(LangParserState& state) {
		return ListParsingTemplate::parseList<
			UniversalExprHolder,
			ArrayLiteralList,
			false,  // empty list allowed
			true,   // trailing separator allowed
			lexer::Token::BracketType::Square,
			internal::Conditions::isComma,
			internal::Conditions::isSentinel,
			internal::NameGetters::arrayLiteralList>(state);
	}
}
